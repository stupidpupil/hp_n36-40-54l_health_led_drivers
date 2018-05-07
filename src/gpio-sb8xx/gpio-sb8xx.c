/*
 * GPIO driver for AMD SB8XX/SB9XX/A5X/A8X south bridges
 *
 * Copyright (c) 2015 Tobias Diedrich
 *
 * Based on the AMD 8111 GPIO driver:
 * Copyright (c) 2012 Dmitry Eremin-Solenikov
 *
 * Based on the AMD RNG driver:
 * Copyright 2005 (c) MontaVista Software, Inc.
 * with the majority of the code coming from:
 *
 * Hardware driver for the Intel/AMD/VIA Random Number Generators (RNG)
 * (c) Copyright 2003 Red Hat Inc <jgarzik@redhat.com>
 *
 * derived from
 *
 * Hardware driver for the AMD 768 Random Number Generator (RNG)
 * (c) Copyright 2001 Red Hat Inc
 *
 * derived from
 *
 * Hardware driver for Intel i810 Random Number Generator (RNG)
 * Copyright 2000,2001 Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2000,2001 Philipp Rumpf <prumpf@mandrakesoft.com>
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>

#define PM_REG_BASE  0xCD6
#define PM_REG_SIZE  2
#define PM_IDX_REG (PM_REG_BASE + 0)
#define PM_DATA_REG  (PM_REG_BASE + 1)
#define PM_ACPI_MMIO_BAR 0x24

#define GPIO_OFFSET  0x100
#define GPIO_SIZE  0x100
#define IOMUX_OFFSET 0xd00
#define IOMUX_SIZE 0x100

#define MAX_GPIO 256

#define REG_GPIO(i)  (0x00 + (i))

#define IOMUX_MASK 0x03
#define IOMUX_FN0  0x00
#define IOMUX_FN1  0x01
#define IOMUX_FN2  0x02
#define IOMUX_FN3  0x03

#define GPIO_OWN_NONE  0x00 /* No owner */
#define GPIO_OWN_IMC 0x01 /* OwnedByImc */
#define GPIO_OWN_HOST  0x02 /* OwnedByHost */
#define GPIO_OWN_MASK  (GPIO_OWN_IMC | GPIO_OWN_HOST)
#define GPIO_STICKY  0x04 /* Settings sticky across reset */
#define GPIO_PULLUP_B  0x08 /* 0 to enable pull-up */
#define GPIO_PULLDOWN  0x10 /* 1 to enable pull-down */
#define GPIO_PULL_MASK (GPIO_PULLUP_B | GPIO_PULLDOWN)
#define GPIO_PULL_UP 0x00
#define GPIO_PULL_DOWN 0x18
#define GPIO_PULL_NONE 0x08
#define GPIO_OUT_EN_B  0x20 /* 0 to enable output */
#define GPIO_OUT 0x40
#define GPIO_IN    0x80

static int enable_unsafe_direction_change;
module_param(enable_unsafe_direction_change, int, 0);
MODULE_PARM_DESC(enable_unsafe_direction_change,
    "Enable gpio direction changes. This can be unsafe!");

static const struct pci_device_id pci_tbl[] = {
 { PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_HUDSON2_SMBUS) },
 { PCI_DEVICE(PCI_VENDOR_ID_ATI, PCI_DEVICE_ID_ATI_SBX00_SMBUS) },
 { PCI_DEVICE(1002, 4385) },
 { 0, }, /* terminate list */
};

/*
 * Explicitly register the supported PCI ids via MODULE_DEVICE_TABLE.
 * We do not actually register a pci_driver, because the SMBUS driver
 * (i2c_piix4) already does.
 */
MODULE_DEVICE_TABLE(pci, pci_tbl);

struct amd_sb8xx_gpio {
 struct gpio_chip  chip;
 u32     gpio_base;
 u32     iomux_base;
 void __iomem    *gpio;
 void __iomem    *iomux;
 struct device   *dev;
 spinlock_t    lock; /* guards hw registers and orig table */
 u8      orig[MAX_GPIO];
};

#define to_agp(chip) container_of(chip, struct amd_sb8xx_gpio, chip)

static int amd_sb8xx_gpio_request(struct gpio_chip *chip, unsigned offset)
{
 struct amd_sb8xx_gpio *agp = to_agp(chip);
 u8 orig = ioread8(agp->gpio + offset);

 if (orig & GPIO_OWN_IMC) {
   dev_err(agp->dev, "Requested GPIO %d is owned by IMC\n",
     offset);
   return -EINVAL;
 }

 agp->orig[offset] = orig;
 dev_dbg(agp->dev, "Requested GPIO %d, data %x\n", offset, orig);
 return 0;
}

static void amd_sb8xx_gpio_free(struct gpio_chip *chip, unsigned offset)
{
 struct amd_sb8xx_gpio *agp = to_agp(chip);

 dev_dbg(agp->dev, "Freed GPIO %d, data %x\n", offset,
   agp->orig[offset]);

 iowrite8(agp->orig[offset], agp->gpio + offset);
}

static void amd_sb8xx_gpio_set(struct gpio_chip *chip, unsigned offset,
            int value)
{
 struct amd_sb8xx_gpio *agp = to_agp(chip);
 u8 temp;
 unsigned long flags;

 spin_lock_irqsave(&agp->lock, flags);
 temp = ioread8(agp->gpio + offset);
 if ((temp & GPIO_OUT_EN_B) == 0 || enable_unsafe_direction_change) {
   temp &= ~(GPIO_OUT | GPIO_OUT_EN_B);
   temp |= value ? GPIO_OUT : 0;
   iowrite8(temp, agp->gpio + offset);
 }
 spin_unlock_irqrestore(&agp->lock, flags);
}

static int amd_sb8xx_gpio_get(struct gpio_chip *chip, unsigned offset)
{
 struct amd_sb8xx_gpio *agp = to_agp(chip);
 u8 temp;

 temp = ioread8(agp->gpio + offset);

 return (temp & GPIO_IN) ? 1 : 0;
}

static int amd_sb8xx_gpio_get_direction(struct gpio_chip *chip,
         unsigned offset)
{
 struct amd_sb8xx_gpio *agp = to_agp(chip);
 u8 temp;

 temp = ioread8(agp->gpio + offset);

 return (temp & GPIO_OUT_EN_B) ? GPIOF_DIR_IN : GPIOF_DIR_OUT;
}

static int amd_sb8xx_gpio_dirout(struct gpio_chip *chip, unsigned offset,
        int value)
{
 struct amd_sb8xx_gpio *agp = to_agp(chip);
 u8 temp, orig;
 unsigned long flags;
 int ret = 0;

 // HACK - Some pins already seem to be set to output
 // but aren't marked as safe for being set to output...
 if (amd_sb8xx_gpio_get_direction(chip, offset) == GPIOF_DIR_OUT) {
  return 0;
 }

 spin_lock_irqsave(&agp->lock, flags);
 orig = ioread8(agp->gpio + offset);
 temp = (orig & ~(GPIO_OUT | GPIO_OUT_EN_B)) | (value ? GPIO_OUT : 0);
 if (orig != temp) {
   if (!enable_unsafe_direction_change)
     ret = -EINVAL;
   else
     iowrite8(temp, agp->gpio + offset);
 }
 spin_unlock_irqrestore(&agp->lock, flags);

 if (ret)
   dev_err(agp->dev,
     "Cannot change GPIO %d to output: enable_unsafe_direction_change not set\n",
     offset);

 return ret;
}

static int amd_sb8xx_gpio_dirin(struct gpio_chip *chip, unsigned offset)
{
 struct amd_sb8xx_gpio *agp = to_agp(chip);
 u8 temp, orig;
 unsigned long flags;
 int ret = 0;

 spin_lock_irqsave(&agp->lock, flags);
 orig = ioread8(agp->gpio + offset);
 temp = orig | GPIO_OUT_EN_B;
 if (orig != temp) {
   if (!enable_unsafe_direction_change)
     ret = -EINVAL;
   else
     iowrite8(temp, agp->gpio + offset);
 }
 spin_unlock_irqrestore(&agp->lock, flags);

 if (ret)
   dev_err(agp->dev,
     "Cannot change GPIO %d to input: enable_unsafe_direction_change not set\n",
     offset);
 return ret;
}

#ifdef CONFIG_DEBUG_FS
static void amd_sb8xx_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
 struct amd_sb8xx_gpio *agp = to_agp(chip);
 int i;

 for (i = 0; i < chip->ngpio; i++) {
   int gpio = i + chip->base;
   u8 reg;
   const char *label, *pull, *owner, *mux;

   /* We report the GPIO even if it's not requested since
    * we're also reporting things like alternate
    * functions which apply even when the GPIO is not in
    * use as a GPIO.
    */
   label = gpiochip_is_requested(chip, i);
   if (!label) {
     label = "Unrequested";

     /* Skip known gaps in the gpio range unless they were
      * explicitly requested.
      */
     if ((i > 67 && i < 96) ||
         (i > 119 && i < 128) ||
         (i > 150 && i < 160) ||
         i > 228)
       continue;
   }

   seq_printf(s, " gpio-%-3d(%d) (%-20.20s) ", gpio, i, label);

   reg = ioread8(agp->gpio + i);

   switch (reg & GPIO_PULL_MASK) {
   case GPIO_PULL_NONE:
     pull = "nopull";
     break;
   case GPIO_PULL_DOWN:
     pull = "pulldown";
     break;
   case GPIO_PULL_UP:
     pull = "pullup";
     break;
   default:
     pull = "INVALID PULL";
     break;
   }

   switch (reg & GPIO_OWN_MASK) {
   case GPIO_OWN_IMC:
     owner = "imc";
     break;
   case GPIO_OWN_HOST:
     owner = "host";
     break;
   case GPIO_OWN_NONE:
     owner = NULL;
     break;
   default:
     owner = "INVALID OWNER";
   }

   reg = ioread8(agp->iomux + i);
   switch (reg & IOMUX_MASK) {
   case IOMUX_FN0:
     mux = "fn0";
     break;
   case IOMUX_FN1:
     mux = "fn1";
     break;
   case IOMUX_FN2:
     mux = "fn2";
     break;
   case IOMUX_FN3:
     mux = "fn3";
     break;
   default:
     mux = "INVALID IOMUX";
     break;
   }

   seq_printf(s, " %s %s %s %s",
        reg & GPIO_OUT_EN_B ? "in" : "out",
        reg & GPIO_IN ? "high" : "low",
        pull, mux);
   if (i >= 96 && i <= 119)
     seq_printf(s, " gevent%d", i - 96);
   if (owner)
     seq_printf(s, " %s", owner);
   seq_putc(s, '\n');
 }
}
#else
#define amd_sb8xx_gpio_dbg_show NULL
#endif

static struct amd_sb8xx_gpio gp = {
 .chip = {
   .label    = "AMD SB8XX/SB9XX/A5X/A8X GPIO driver",
   .owner    = THIS_MODULE,
   .base   = -1,
   .ngpio    = MAX_GPIO,
   .dbg_show = amd_sb8xx_gpio_dbg_show,
   .request  = amd_sb8xx_gpio_request,
   .free   = amd_sb8xx_gpio_free,
   .set    = amd_sb8xx_gpio_set,
   .get    = amd_sb8xx_gpio_get,
   .get_direction  = amd_sb8xx_gpio_get_direction,
   .direction_output = amd_sb8xx_gpio_dirout,
   .direction_input = amd_sb8xx_gpio_dirin,
 },
};

static u8 amd_pm_read(u8 reg)
{
 outb_p(reg, PM_IDX_REG);
 return inb_p(PM_DATA_REG);
}

static u32 amd_acpi_mmio_base(struct device *dev)
{
 u32 bar = 0;
 struct resource *pm_region;

 pm_region = devm_request_region(dev, PM_REG_BASE, PM_REG_SIZE,
         "AMD PM IO");
 if (!pm_region) {
   dev_err(dev, "AMD PM IO already in use.\n");
   return 0;
 }

 bar = amd_pm_read(PM_ACPI_MMIO_BAR);
 bar |= amd_pm_read(PM_ACPI_MMIO_BAR + 1) << 8;
 bar |= amd_pm_read(PM_ACPI_MMIO_BAR + 2) << 16;
 bar |= amd_pm_read(PM_ACPI_MMIO_BAR + 3) << 24;

 if (bar == 0x00000000 || bar == 0xffffffff) {
   dev_err(dev, "Could not read AMD ACPI MMIO base address.\n");
   bar = 0;
   goto out_release_pmio;
 }
 if ((bar & 3) != 1) {
   dev_err(dev, "ACPI registers are not memory mapped.\n");
   bar = 0;
   goto out_release_pmio;
 }

out_release_pmio:
 devm_release_region(dev, PM_REG_BASE, PM_REG_SIZE);

 return bar & ~3;
}

static struct pci_dev *amd_get_sb_pcidev(void)
{
 struct pci_dev *dev = NULL;
 const struct pci_device_id *ent;

 /* We look for our device - AMD South Bridge
  * I don't know about a system with two such bridges,
  * so we can assume that there is max. one device.
  *
  * We can't use plain pci_driver mechanism,
  * as the device is really a multiple function device,
  * main driver that binds to the pci_device is an smbus
  * driver and have to find & bind to the device this way.
  */
 for_each_pci_dev(dev) {
   ent = pci_match_id(pci_tbl, dev);
   if (ent)
     return dev;
 }

 return NULL;
}

static int __init amd_sb8xx_gpio_init(void)
{
 int ret = -ENODEV;
 struct pci_dev *pcidev = amd_get_sb_pcidev();
 u32 acpi_bar = 0;

 if (!pcidev)
   goto err;

 if (pcidev->device == PCI_DEVICE_ID_ATI_SBX00_SMBUS &&
     pcidev->revision < 0x40) {
   /* Southbridges before SB800 handle GPIO differently. */
   goto err;
 }

 gp.dev = &pcidev->dev;

 acpi_bar = amd_acpi_mmio_base(gp.dev);
 if (!acpi_bar) {
   dev_err(gp.dev, "Unable to find MMIO base address!\n");
   goto err;
 }

 gp.gpio_base = (acpi_bar & ~3) + GPIO_OFFSET;
 gp.iomux_base = (acpi_bar & ~3) + IOMUX_OFFSET;

 if (!devm_request_mem_region(gp.dev, gp.gpio_base, GPIO_SIZE,
            "AMD SB8XX GPIO")) {
   dev_err(gp.dev, "GPIO region 0x%x already in use!\n",
     gp.gpio_base);
   ret = -EBUSY;
   goto err;
 }
 if (!devm_request_mem_region(gp.dev, gp.iomux_base, IOMUX_SIZE,
            "AMD SB8XX IOMUX")) {
   dev_err(gp.dev, "IOMUX region 0x%x already in use!\n",
     gp.iomux_base);
   ret = -EBUSY;
   goto err_release_gpio;
 }

 gp.gpio = devm_ioremap(gp.dev, gp.gpio_base, GPIO_SIZE);
 if (!gp.gpio) {
   dev_err(gp.dev, "Couldn't map GPIO region\n");
   ret = -ENOMEM;
   goto err_release_iomux;
 }
 gp.iomux = devm_ioremap(gp.dev, gp.iomux_base, IOMUX_SIZE);
 if (!gp.gpio) {
   dev_err(gp.dev, "Couldn't map IOMUX region\n");
   ret = -ENOMEM;
   goto err_release_iomux;
 }


 gp.dev = gp.dev;
 //gp.chip.dev = gp.dev;

 spin_lock_init(&gp.lock);

 ret = gpiochip_add(&gp.chip);
 if (ret) {
   dev_err(gp.dev, "Registering gpiochip failed\n");
   goto err_release_iomux;
 }

 return 0;

err_release_iomux:
 devm_release_mem_region(gp.dev, gp.iomux_base, GPIO_SIZE);
err_release_gpio:
 devm_release_mem_region(gp.dev, gp.gpio_base, GPIO_SIZE);
err:
 return ret;
}

static void __exit amd_sb8xx_gpio_exit(void)
{
 gpiochip_remove(&gp.chip);
 devm_release_mem_region(gp.dev, gp.iomux_base, GPIO_SIZE);
 devm_release_mem_region(gp.dev, gp.gpio_base, GPIO_SIZE);
}

module_init(amd_sb8xx_gpio_init);
module_exit(amd_sb8xx_gpio_exit);

MODULE_AUTHOR("Tobias Diedrich");
MODULE_DESCRIPTION("GPIO driver for AMD SB8XX/SB9XX/A5X/A8X chipsets");
MODULE_LICENSE("GPL");

