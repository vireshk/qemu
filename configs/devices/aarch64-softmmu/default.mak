# Default configuration for aarch64-softmmu

# We support all the 32 bit boards so need all their config
include ../arm-softmmu/default.mak

CONFIG_DS1338=y
CONFIG_TMP105=y
CONFIG_IMX_I2C=y
CONFIG_ARM_SBCON_I2C=y
CONFIG_VIRTIO_NET=y
CONFIG_ARM_GICV3_TCG=y
