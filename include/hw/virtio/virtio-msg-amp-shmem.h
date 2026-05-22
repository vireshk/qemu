/*
 * Virtio-msg AMP shared memory header.
 *
 * Copyright (c) 2026 Advanced Micro Devices, Inc and Linaro.
 * Viresh Kumar <viresh.kumar@linaro.org>
 *
 * This file defines the self-describing layout placed at the beginning of the
 * shared memory region used for communication between peers.
 *
 * The layout allows both sides to discover:
 *  - notification areas
 *  - queue heads
 *  - message buffers
 *
 * All offsets are relative to the start of the shared memory region unless
 * specified otherwise.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_VIRTIO_MSG_AMP_SHMEM_H
#define QEMU_VIRTIO_MSG_AMP_SHMEM_H

#define VMAMP_SHMEM_PKT_SIZE    64

/**
 * struct VmampShmemQueueHead - per-endpoint queue head.
 * @magic: Validity indicator for the queue head. Must be
 *	   VMAMP_SHMEM_QUEUE_MAGIC_READY when valid.
 * @status: Current state of the queue.
 * @tail: Index of next element to be written (producer index).
 * @head: Index of next element to be read (consumer index).
 *
 * The queue is a circular buffer of @num_queue_elements entries, where one entry is
 * always kept unused.
 *
 * - head and tail are written only by this peer.
 * - head and tail are always < num_queue_elements
 * - empty: head == peer->tail
 * - full:  (tail + 1) % num_queue_elements == peer->head
 *
 * The separation of head and tail enables lock-free operation.
 */
typedef struct VmampShmemQueueHead {
    #define VMAMP_SHMEM_QUEUE_MAGIC_READY       0x514F /* "QO - Queue OK" */
    uint16_t magic;

    #define VMAMP_SHMEM_QUEUE_STATE_MASK        0x001F
    #define VMAMP_SHMEM_QUEUE_STATE_INIT        0x0
    #define VMAMP_SHMEM_QUEUE_STATE_READY       0x1
    #define VMAMP_SHMEM_QUEUE_STATE_RUN	        0x2
    #define VMAMP_SHMEM_QUEUE_STATE_SHUTDOWN    0x3
    #define VMAMP_SHMEM_QUEUE_STATE_PEER_DEAD   0x4
    #define VMAMP_SHMEM_QUEUE_FLAG_MASK         0xF000
    #define VMAMP_SHMEM_QUEUE_FLAG_DEBUGGABLE   0x8000
    uint16_t status;
    uint16_t tail;
    uint16_t head;
} VmampShmemQueueHead;

/*
 * struct VmampShmemNotif - Notification bitmap arrays
 * @tx: Bits set by this peer to notify the other side.
 * @rx: Bits set by this peer in response to other peer's tx bits.
 * @mask: Optional (if VMAMP_SHMEM_FEAT_MASKED_NOTIF is set), mask bits to
 *        suppress interrupt generation.
 *
 * Each array has `num_notif` elements.
 * Each element is a 64-bit word (64 notification bits).
 *
 * Pending events are computed as:
 *
 *   pending = rx[n] ^ peer->tx[n]
 *
 * Acknowledgment:
 *
 *   rx[n] ^= pending
 *
 * Hierarchical notification schemes may be implemented using multiple words.
 * };
 */
typedef struct VmampShmemNotif {
    uint64_t *tx;
    uint64_t *rx;
    uint64_t *mask;
} VmampShmemNotif;


/**
 * struct VmampShmemLayout - Self-describing shared memory layout
 * @length: Size of this structure in bytes.
 * @magic: Validity indicator for the layout. Must be VMAMP_SHMEM_MAGIC_READY
 *         when valid.
 * @version: Layout version.
 * @features: Optional features supported by this layout (e.g. masked notifications).
 * @num_notif: Number of notification words (uint64_t) per direction.
 *         Each word provides 64 notification bits.
 * @num_vq_notif_per_device: Number of per-device virtqueue notification bits.
 *         Supported values: 1, 2, 4, 8, 16.
 * @num_queue_elements: Number of elements in each message queue.
 * @size_queue_elements: Size (in bytes) of each queue element.
 * @reserved: Reserved for future use. Must be zero.
 *
 * @dev_notif_offset: Offset to device-owned notification area.
 *         Contains tx/rx (and optional mask) arrays for the device.
 * @dev_queue_head_offset: Offset to device queue head structure.
 * @dev_queue_elements_offset: Offset to device message buffer (queue elements).
 *
 * @drv_notif_offset: Offset to driver-owned notification area.
 * @drv_queue_head_offset: Offset to driver queue head structure.
 * @drv_queue_elements_offset: Offset to driver message buffer (queue elements).
 *
 * The shared memory is split logically into two halves:
 *  - Device-owned region (device writes, driver reads)
 *  - Driver-owned region (driver writes, device reads)
 *
 * Each side updates only its own structures to maintain lock-free operation.
 */
typedef struct VmampShmemLayout {
    #define VMAMP_SHMEM_MAGIC_READY     0x4C4F /* "LO - Layout OK" */
    uint16_t magic;
    uint16_t length;

    #define VMAMP_SHMEM_VERSION_1       0x1
    uint16_t version;

    #define VMAMP_SHMEM_FEAT_MASKED_NOTIF       0x1
    uint16_t features;

    #define VMAMP_SHMEM_NUM_NOTIF_MAX           64
    uint8_t num_notif;
    uint8_t num_vq_notif_per_device;

    uint16_t num_queue_elements;
    uint16_t size_queue_elements;
    uint16_t reserved;

    uint64_t dev_notif_offset;
    uint64_t dev_queue_head_offset;
    uint64_t dev_queue_elements_offset;

    uint64_t drv_notif_offset;
    uint64_t drv_queue_head_offset;
    uint64_t drv_queue_elements_offset;
} QEMU_PACKED VmampShmemLayout;

typedef struct VmampShmem {
    VmampShmemLayout *layout;
    uint64_t layout_size;
    VmampShmemNotif my_notif;
    VmampShmemNotif peer_notif;
    VmampShmemQueueHead *my_qh;
    VmampShmemQueueHead *peer_qh;
    uint8_t *my_queue;
    uint8_t *peer_queue;
    bool masked_notif;
    uint16_t size_queue_elements;
    uint16_t num_queue_elements;
    uint8_t num_notif;
    uint8_t num_vq_notif_per_device;
} VmampShmem;

typedef struct VmampShmemRegion {
    uint64_t offset;
    size_t size;
} VmampShmemRegion;

#define VMAMP_SHMEM_SUPPORTED_FEATURES VMAMP_SHMEM_FEAT_MASKED_NOTIF
#define VMAMP_SHMEM_QH_ALIGNMENT       sizeof(uint16_t)
#define VMAMP_SHMEM_NOTIF_ALIGNMENT    sizeof(uint64_t)
#define VMAMP_SHMEM_QUEUE_ALIGNMENT    1

static inline uint16_t get_val(uint16_t *ptr)
{
    uint16_t val;

    val = le16_to_cpu(qatomic_read(ptr));

    /* Make sure the read is completed before we return */
    smp_mb_acquire();
    return val;
}

static inline void set_val(uint16_t *ptr, uint16_t val)
{
    /* Make sure earlier writes are finished before this one */
    smp_mb_release();

    qatomic_set(ptr, cpu_to_le16(val));
}

static inline uint16_t next_entry(uint16_t idx, uint16_t size)
{
    return (idx + 1 == size) ? 0 : idx + 1;
}

static inline uint16_t get_queue_magic(VmampShmemQueueHead *qh)
{
    return get_val(&qh->magic);
}

static inline void set_queue_magic(VmampShmemQueueHead *qh, uint16_t val)
{
    set_val(&qh->magic, val);
}

static inline uint16_t get_queue_status(VmampShmemQueueHead *qh)
{
    return get_val(&qh->status) & VMAMP_SHMEM_QUEUE_STATE_MASK;
}

static inline void set_queue_status(VmampShmemQueueHead *qh, uint16_t val)
{
    set_val(&qh->status, val & VMAMP_SHMEM_QUEUE_STATE_MASK);
}

static inline uint16_t get_queue_tail(VmampShmemQueueHead *qh)
{
    return get_val(&qh->tail);
}

static inline void set_queue_tail(VmampShmemQueueHead *qh, uint16_t val)
{
    set_val(&qh->tail, val);
}

static inline uint16_t get_queue_head(VmampShmemQueueHead *qh)
{
    return get_val(&qh->head);
}

static inline void set_queue_head(VmampShmemQueueHead *qh, uint16_t val)
{
    set_val(&qh->head, val);
}

static inline bool vmamp_shmem_valid_vq_notif_count(uint8_t count)
{
    return count == 1 || count == 2 || count == 4 || count == 8 ||
           count == 16;
}

static inline int vmamp_shmem_validate_region(VmampShmem *shmem,
        uint64_t offset, size_t size, size_t alignment)
{
    if (!offset || !size || !QEMU_IS_ALIGNED(offset, alignment)) {
        return -EINVAL;
    }

    if (offset > shmem->layout_size || size > shmem->layout_size - offset) {
        return -EINVAL;
    }

    return 0;
}

static inline bool vmamp_shmem_regions_overlap(const VmampShmemRegion *a,
        const VmampShmemRegion *b)
{
    return a->offset < b->offset + b->size &&
           b->offset < a->offset + a->size;
}

static inline int vmamp_shmem_validate_regions(const VmampShmemRegion *regions,
        int nr)
{
    int i, j;

    for (i = 0; i < nr; i++) {
        for (j = i + 1; j < nr; j++) {
            if (vmamp_shmem_regions_overlap(&regions[i], &regions[j])) {
                return -EINVAL;
            }
        }
    }

    return 0;
}

static inline int vmamp_shmem_send(VmampShmem *shmem, void *buf, size_t size)
{
    VmampShmemQueueHead *myqh = shmem->my_qh;
    VmampShmemQueueHead *peerqh = shmem->peer_qh;
    uint16_t tail, next_tail;

    if (unlikely(!size || size > shmem->size_queue_elements))
        return -EINVAL;

    tail = get_queue_tail(myqh);
    next_tail = next_entry(tail, shmem->num_queue_elements);

    /* Queue full ? */
    if (unlikely(next_tail == get_queue_head(peerqh)))
        return -EAGAIN;

    memcpy(shmem->my_queue + shmem->size_queue_elements * tail, buf, size);
    set_queue_tail(myqh, next_tail);
    return 0;
}

static inline int vmamp_shmem_recv(VmampShmem *shmem, void *buf, size_t size)
{
    VmampShmemQueueHead *myqh = shmem->my_qh;
    VmampShmemQueueHead *peerqh = shmem->peer_qh;
    uint16_t head;

    if (unlikely(!size || size > shmem->size_queue_elements))
        return -EINVAL;

    head = get_queue_head(myqh);

    /* Queue empty ? */
    if (unlikely(head == get_queue_tail(peerqh)))
        return -EAGAIN;

    memcpy(buf, shmem->peer_queue + shmem->size_queue_elements * head, size);
    set_queue_head(myqh, next_entry(head, shmem->num_queue_elements));
    return 0;
}

static inline int vmamp_shmem_queue_try_init(VmampShmem *shmem)
{
    VmampShmemQueueHead *myqh = shmem->my_qh;
    VmampShmemQueueHead *peerqh = shmem->peer_qh;
    uint16_t val;

    if (get_queue_magic(myqh) != VMAMP_SHMEM_QUEUE_MAGIC_READY) {
        set_queue_status(myqh, VMAMP_SHMEM_QUEUE_STATE_INIT);
        set_queue_magic(myqh, VMAMP_SHMEM_QUEUE_MAGIC_READY);
    }

    if (get_queue_magic(peerqh) != VMAMP_SHMEM_QUEUE_MAGIC_READY)
        return -EAGAIN;

    if (get_queue_status(peerqh) > VMAMP_SHMEM_QUEUE_STATE_RUN)
        goto shutdown;

    if (get_queue_status(myqh) == VMAMP_SHMEM_QUEUE_STATE_INIT) {
        set_queue_head(myqh, 0);
        set_queue_tail(myqh, 0);
        set_queue_status(myqh, VMAMP_SHMEM_QUEUE_STATE_READY);
    }

    val = get_queue_status(peerqh);

    if (val == VMAMP_SHMEM_QUEUE_STATE_INIT)
        return -EAGAIN;

    if (unlikely(val > VMAMP_SHMEM_QUEUE_STATE_RUN))
        goto shutdown;

    set_queue_status(myqh, VMAMP_SHMEM_QUEUE_STATE_RUN);

    return 0;

shutdown:
    set_queue_status(myqh, VMAMP_SHMEM_QUEUE_STATE_SHUTDOWN);
    return -EIO;
}

static inline void vmamp_shmem_queue_shutdown(VmampShmem *shmem)
{
    if (shmem->my_qh) {
        set_queue_status(shmem->my_qh, VMAMP_SHMEM_QUEUE_STATE_SHUTDOWN);
    }
}

static inline int vmamp_shmem_dev_init(VmampShmem *shmem,
        VmampShmemLayout *layout, size_t mapsize)
{
    size_t offset, count;

    if (!layout)
        return -ENOMEM;

    offset = sizeof(*layout) + 2 * sizeof(VmampShmemQueueHead);
    offset = QEMU_ALIGN_UP(offset, sizeof(uint64_t));
    if (mapsize < offset + 2 * VMAMP_SHMEM_PKT_SIZE) {
        return -EINVAL;
    }

    count = (mapsize - offset) / (2 * VMAMP_SHMEM_PKT_SIZE);
    if (count < 2) {
        return -EINVAL;
    }
    if (count > UINT16_MAX) {
        return -EINVAL;
    }

    shmem->layout = layout;
    shmem->layout_size = mapsize;
    shmem->my_qh = (void *)(layout + 1);
    shmem->peer_qh = shmem->my_qh + 1;
    shmem->my_queue = (void *)((uint8_t *)layout + offset);
    shmem->peer_queue = (void *)((uint8_t *)shmem->my_queue + count * VMAMP_SHMEM_PKT_SIZE);
    shmem->masked_notif = false;
    shmem->size_queue_elements = VMAMP_SHMEM_PKT_SIZE;
    shmem->num_queue_elements = count;
    shmem->num_notif = 0;
    shmem->num_vq_notif_per_device = 0;

    /* Initialize the driver area */
    memset(shmem->my_queue, 0,
            shmem->num_queue_elements * shmem->size_queue_elements);

    layout->length = cpu_to_le16(sizeof(*layout));
    layout->version = cpu_to_le16(VMAMP_SHMEM_VERSION_1);
    layout->features = 0;
    layout->num_notif = shmem->num_notif;
    layout->num_vq_notif_per_device = shmem->num_vq_notif_per_device;
    layout->size_queue_elements = cpu_to_le16(shmem->size_queue_elements);
    layout->num_queue_elements = cpu_to_le16(shmem->num_queue_elements);
    layout->dev_notif_offset = 0;
    layout->drv_notif_offset = 0;
    layout->reserved = 0;

    layout->dev_queue_head_offset = cpu_to_le64((uint8_t *)shmem->my_qh - (uint8_t *)layout);
    layout->drv_queue_head_offset = cpu_to_le64((uint8_t *)shmem->peer_qh - (uint8_t *)layout);
    layout->dev_queue_elements_offset = cpu_to_le64((uint8_t *)shmem->my_queue - (uint8_t *)layout);
    layout->drv_queue_elements_offset = cpu_to_le64((uint8_t *)shmem->peer_queue - (uint8_t *)layout);

    /* Make sure earlier writes are finished before this one */
    smp_mb_release();
    layout->magic = cpu_to_le16(VMAMP_SHMEM_MAGIC_READY);

    return 0;
}

static inline int vmamp_shmem_drv_init(VmampShmem *shmem,
        VmampShmemLayout *layout, size_t mapsize)
{
    uint64_t dev_notif_offset, drv_notif_offset;
    uint64_t dev_qh_offset, drv_qh_offset;
    uint64_t dev_queue_offset, drv_queue_offset;
    VmampShmemRegion regions[7];
    size_t notif_size = 0, queue_size;
    uint16_t magic;
    uint16_t features;
    int nr = 0, ret;

    if (!layout)
        return -ENOMEM;

    if (mapsize < sizeof(*layout)) {
        return -EINVAL;
    }

    shmem->layout = layout;
    shmem->layout_size = mapsize;

    magic = le16_to_cpu(layout->magic);

    /* Make sure the read is completed before we return */
    smp_mb_acquire();

    if (magic != VMAMP_SHMEM_MAGIC_READY)
        return -EAGAIN;

    /*
     * Once the magic number is set, the peer will not update rest of the
     * fields in layout. Don't need READ_ONCE() for rest of the reads.
     */
    if (le16_to_cpu(layout->length) != sizeof(*layout))
        return -EINVAL;

    if (le16_to_cpu(layout->version) != VMAMP_SHMEM_VERSION_1)
        return -EINVAL;

    features = le16_to_cpu(layout->features);
    if (features & ~VMAMP_SHMEM_SUPPORTED_FEATURES) {
        return -EINVAL;
    }

    if (le16_to_cpu(layout->reserved)) {
        return -EINVAL;
    }

    if (layout->num_notif > VMAMP_SHMEM_NUM_NOTIF_MAX) {
        return -EINVAL;
    }

    if (layout->num_notif &&
        !vmamp_shmem_valid_vq_notif_count(layout->num_vq_notif_per_device)) {
        return -EINVAL;
    }

    shmem->size_queue_elements = le16_to_cpu(layout->size_queue_elements);
    shmem->num_queue_elements = le16_to_cpu(layout->num_queue_elements);

    if (!shmem->size_queue_elements || shmem->num_queue_elements < 2)
        return -EINVAL;

    if (shmem->num_queue_elements >
        SIZE_MAX / shmem->size_queue_elements) {
        return -EINVAL;
    }
    queue_size = shmem->num_queue_elements * shmem->size_queue_elements;

    if (layout->num_notif) {
        shmem->num_notif = layout->num_notif;
        shmem->masked_notif = features & VMAMP_SHMEM_FEAT_MASKED_NOTIF;
        shmem->num_vq_notif_per_device = layout->num_vq_notif_per_device;
        if (shmem->num_notif >
            SIZE_MAX / sizeof(*shmem->my_notif.tx) /
            (shmem->masked_notif ? 3 : 2)) {
            return -EINVAL;
        }
        notif_size = shmem->num_notif * sizeof(*shmem->my_notif.tx) *
                     (shmem->masked_notif ? 3 : 2);
    } else {
        shmem->num_notif = 0;
        shmem->masked_notif = false;
        shmem->num_vq_notif_per_device = 0;
    }

    dev_qh_offset = le64_to_cpu(layout->dev_queue_head_offset);
    drv_qh_offset = le64_to_cpu(layout->drv_queue_head_offset);
    dev_queue_offset = le64_to_cpu(layout->dev_queue_elements_offset);
    drv_queue_offset = le64_to_cpu(layout->drv_queue_elements_offset);
    dev_notif_offset = le64_to_cpu(layout->dev_notif_offset);
    drv_notif_offset = le64_to_cpu(layout->drv_notif_offset);

    ret = vmamp_shmem_validate_region(shmem, dev_qh_offset,
                                      sizeof(*shmem->peer_qh),
                                      VMAMP_SHMEM_QH_ALIGNMENT);
    if (ret) {
        return ret;
    }

    ret = vmamp_shmem_validate_region(shmem, drv_qh_offset,
                                      sizeof(*shmem->my_qh),
                                      VMAMP_SHMEM_QH_ALIGNMENT);
    if (ret) {
        return ret;
    }

    ret = vmamp_shmem_validate_region(shmem, dev_queue_offset, queue_size,
                                      VMAMP_SHMEM_QUEUE_ALIGNMENT);
    if (ret) {
        return ret;
    }

    ret = vmamp_shmem_validate_region(shmem, drv_queue_offset, queue_size,
                                      VMAMP_SHMEM_QUEUE_ALIGNMENT);
    if (ret) {
        return ret;
    }

    if (shmem->num_notif) {
        ret = vmamp_shmem_validate_region(shmem, dev_notif_offset,
                                          notif_size,
                                          VMAMP_SHMEM_NOTIF_ALIGNMENT);
        if (ret) {
            return ret;
        }

        ret = vmamp_shmem_validate_region(shmem, drv_notif_offset,
                                          notif_size,
                                          VMAMP_SHMEM_NOTIF_ALIGNMENT);
        if (ret) {
            return ret;
        }
    }

    regions[nr++] = (VmampShmemRegion){ 0, sizeof(*layout) };
    regions[nr++] = (VmampShmemRegion){ dev_qh_offset,
                                        sizeof(*shmem->peer_qh) };
    regions[nr++] = (VmampShmemRegion){ drv_qh_offset,
                                        sizeof(*shmem->my_qh) };
    regions[nr++] = (VmampShmemRegion){ dev_queue_offset, queue_size };
    regions[nr++] = (VmampShmemRegion){ drv_queue_offset, queue_size };
    if (shmem->num_notif) {
        regions[nr++] = (VmampShmemRegion){ dev_notif_offset, notif_size };
        regions[nr++] = (VmampShmemRegion){ drv_notif_offset, notif_size };
    }

    if (vmamp_shmem_validate_regions(regions, nr)) {
        return -EINVAL;
    }

    shmem->peer_qh = (void *)layout + dev_qh_offset;
    shmem->peer_queue = (void *)layout + dev_queue_offset;

    if (shmem->num_notif) {
        shmem->peer_notif.tx = (void *)layout + dev_notif_offset;

        shmem->peer_notif.rx = shmem->peer_notif.tx + shmem->num_notif;
        if (shmem->masked_notif)
            shmem->peer_notif.mask =
                shmem->peer_notif.rx + shmem->num_notif;
        else
            shmem->peer_notif.mask = NULL;
    }

    shmem->my_qh = (void *)layout + drv_qh_offset;
    shmem->my_queue = (void *)layout + drv_queue_offset;

    if (shmem->num_notif) {
        shmem->my_notif.tx = (void *)layout + drv_notif_offset;
        shmem->my_notif.rx =
            shmem->my_notif.tx + shmem->num_notif;
        if (shmem->masked_notif)
            shmem->my_notif.mask =
                shmem->my_notif.rx + shmem->num_notif;
        else
            shmem->my_notif.mask = NULL;
    }

    /* Initialize the driver area */
    memset(shmem->my_queue, 0,
            shmem->num_queue_elements * shmem->size_queue_elements);
    if (shmem->num_notif) {
        memset(shmem->my_notif.tx, 0,
                shmem->num_notif * sizeof(*shmem->my_notif.tx) *
                (shmem->masked_notif ? 3 : 2));
    }

    return 0;
}

static inline int vmamp_shmem_init(VmampShmem *shmem, VmampShmemLayout *layout,
        size_t mapsize, bool is_device)
{
    if (is_device) {
        return vmamp_shmem_dev_init(shmem, layout, mapsize);
    } else {
        return vmamp_shmem_drv_init(shmem, layout, mapsize);
    }
}
#endif /* QEMU_VIRTIO_MSG_AMP_SHMEM_H */
