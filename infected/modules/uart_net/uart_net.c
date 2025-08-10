#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/etherdevice.h>
#include <linux/amba/serial.h>

#define DRV_NAME "uartnet"
#define UARTNET_MTU 512
#define FRAME_HEADER 0xAA
#define FRAME_FOOTER 0x55
#define ESCAPE_CHAR 0x5A
#define NAPI_WEIGHT 64
#define TX_QUEUE_LEN 1

#if defined (UART_DEBUG_ENABLE)
#define UART_NET_DEBUG(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#else
#define UART_NET_DEBUG(fmt, ...)
#endif

// 接收状态机状态
enum uartnet_rx_state {
    UARTNET_STATE_IDLE,
    UARTNET_STATE_LENGTH,
    UARTNET_STATE_DATA,
    UARTNET_STATE_ESCAPE,
    UARTNET_STATE_END
};

// 发送状态机状态
enum uartnet_tx_state {
    TX_STATE_IDLE,
    TX_STATE_HEADER,
    TX_STATE_LENGTH,
    TX_STATE_DATA,
    TX_STATE_FOOTER
};

struct uartnet_priv {
    struct net_device *netdev;
    void __iomem *uart_base;
    int irq;
    struct napi_struct napi;
    
    // 接收状态机
    enum uartnet_rx_state rx_state;
    u8 saved_state;         // 转义前保存的状态
    struct sk_buff *rx_skb;
    u16 rx_frame_length;    // 数据部分长度（转义前）
    u16 rx_data_received;   // 已接收数据字节数（转义前）
    u8 rx_length_bytes[2];  // 存储原始长度字节
    u8 rx_length_index;     // 长度字段字节计数
    
    spinlock_t lock;

    // 发送状态机
    spinlock_t tx_lock;
    spinlock_t tx_xmit_lock;
    struct sk_buff *tx_skb;
    enum uartnet_tx_state tx_state;
    u8 tx_length_bytes[2];
    const u8 *tx_current_ptr;
    u16 tx_current_count;
    u8 tx_escape_next;

    u32 tx_intr;
    u32 rx_intr;
};

static const struct of_device_id pl011_ids[] = {
    { .compatible = "arm,pl011,uart_net", },
    {}
};
MODULE_DEVICE_TABLE(of, pl011_ids);

/* 处理发送中断 */
static void uartnet_tx_interrupt(struct net_device *dev)
{
    struct uartnet_priv *priv = netdev_priv(dev);
    unsigned long flags;
    u32 fr;

    spin_lock_irqsave(&priv->tx_lock, flags);

    while (true) {
        fr = readl(priv->uart_base + UART01x_FR);
        
        // FIFO已满或没有数据要发送时退出
        if ((fr & UART01x_FR_TXFF) || priv->tx_state == TX_STATE_IDLE)
            break;
        
        // UART_NET_DEBUG("uartnet_tx_interrupt");
        switch (priv->tx_state) {
        case TX_STATE_HEADER:
            writel(FRAME_HEADER, priv->uart_base + UART01x_DR);
            priv->tx_state = TX_STATE_LENGTH;
            break;

        case TX_STATE_LENGTH:
            if (priv->tx_current_count > 0) {
                u8 byte = *priv->tx_current_ptr;
                
                if (priv->tx_escape_next) {
                    writel(byte ^ 0x20, priv->uart_base + UART01x_DR);
                    priv->tx_escape_next = 0;
                    priv->tx_current_ptr++;
                    priv->tx_current_count--;
                } else if (byte == FRAME_HEADER || 
                          byte == FRAME_FOOTER || 
                          byte == ESCAPE_CHAR) {
                    writel(ESCAPE_CHAR, priv->uart_base + UART01x_DR);
                    priv->tx_escape_next = 1;
                } else {
                    writel(byte, priv->uart_base + UART01x_DR);
                    priv->tx_current_ptr++;
                    priv->tx_current_count--;
                }
                
                if (priv->tx_current_count == 0) {
                    priv->tx_state = TX_STATE_DATA;
                    priv->tx_current_ptr = priv->tx_skb->data;
                    priv->tx_current_count = priv->tx_skb->len;
                    priv->tx_escape_next = 0;
                }
            }
            break;

        case TX_STATE_DATA:
            if (priv->tx_current_count > 0) {
                u8 byte = *priv->tx_current_ptr;
                
                if (priv->tx_escape_next) {
                    writel(byte ^ 0x20, priv->uart_base + UART01x_DR);
                    priv->tx_escape_next = 0;
                    priv->tx_current_ptr++;
                    priv->tx_current_count--;
                } else if (byte == FRAME_HEADER || 
                          byte == FRAME_FOOTER || 
                          byte == ESCAPE_CHAR) {
                    writel(ESCAPE_CHAR, priv->uart_base + UART01x_DR);
                    priv->tx_escape_next = 1;
                } else {
                    writel(byte, priv->uart_base + UART01x_DR);
                    priv->tx_current_ptr++;
                    priv->tx_current_count--;
                }
                
                if (priv->tx_current_count == 0)
                    priv->tx_state = TX_STATE_FOOTER;
            }
            break;

        case TX_STATE_FOOTER:
            writel(FRAME_FOOTER, priv->uart_base + UART01x_DR);
            dev->stats.tx_packets++;
            dev->stats.tx_bytes += priv->tx_skb->len;
            dev_kfree_skb(priv->tx_skb);
            priv->tx_skb = NULL;
            priv->tx_state = TX_STATE_IDLE;
            netif_wake_queue(dev);
            goto unlock; // 发送完成，退出循环
        }
    }

unlock:
    // 如果没有更多数据发送，禁用TX中断
    if (priv->tx_state == TX_STATE_IDLE) {
        writel(readl(priv->uart_base + UART011_IMSC) & ~UART011_TXIM, priv->uart_base + UART011_IMSC);
    }
    
    spin_unlock_irqrestore(&priv->tx_lock, flags);
}

/* 网络设备发送函数 */
static netdev_tx_t uartnet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct uartnet_priv *priv = netdev_priv(dev);
    unsigned long flags;
    
    spin_lock_irqsave(&priv->tx_xmit_lock, flags);
    
    if (unlikely(!netif_running(dev) || !netif_device_present(dev))) {
        spin_unlock_irqrestore(&priv->tx_xmit_lock, flags);
        dev_kfree_skb(skb);
        return NETDEV_TX_OK;
    }
    netif_stop_queue(dev);
    
    if (priv->tx_state == TX_STATE_IDLE) {
        // 立即开始发送
        priv->tx_skb = skb;
        priv->tx_state = TX_STATE_HEADER;
        
        // 准备长度字段
        priv->tx_length_bytes[0] = (skb->len >> 8) & 0xFF;
        priv->tx_length_bytes[1] = skb->len & 0xFF;
        priv->tx_current_ptr = priv->tx_length_bytes;
        priv->tx_current_count = sizeof(priv->tx_length_bytes);
        priv->tx_escape_next = 0;
        
        // 启用TX中断并触发第一次发送
        writel(readl(priv->uart_base + UART011_IMSC) | UART011_TXIM, priv->uart_base + UART011_IMSC);
        UART_NET_DEBUG("uartnet_start_xmit");
        // 手动触发发送中断处理
        uartnet_tx_interrupt(dev);
    } else {
        // 队列已满，丢弃数据包（实际应入队，但简单处理）
        dev_kfree_skb(skb);
        dev->stats.tx_dropped++;
        UART_NET_DEBUG ("drop");
        netif_wake_queue(dev); // 恢复队列
    }
    
    spin_unlock_irqrestore(&priv->tx_xmit_lock, flags);
    return NETDEV_TX_OK;
}

/* NAPI poll函数，处理接收数据 */
static int uartnet_poll(struct napi_struct *napi, int budget)
{
    struct uartnet_priv *priv = container_of(napi, struct uartnet_priv, napi);
    struct net_device *dev = priv->netdev;
    int work_done = 0;
    u8 data;
    UART_NET_DEBUG("uartnet_poll");

    while (work_done < budget) {
        /* 检查是否有数据 */
        if (readl(priv->uart_base + UART01x_FR) & UART01x_FR_RXFE) {
            break;
        }
        
        data = readl(priv->uart_base + UART01x_DR) & 0xFF;
    restart:
        switch (priv->rx_state) {
            case UARTNET_STATE_IDLE:
                if (data == FRAME_HEADER) {
                    priv->rx_state = UARTNET_STATE_LENGTH;
                    priv->rx_length_index = 0;
                    priv->rx_frame_length = 0;
                    priv->rx_data_received = 0;
                    priv->rx_skb = NULL;
                }
                break;
                
            case UARTNET_STATE_LENGTH:
            case UARTNET_STATE_DATA:
                if (data == ESCAPE_CHAR) {
                    priv->saved_state = priv->rx_state;
                    priv->rx_state = UARTNET_STATE_ESCAPE;
                } else {
                    /* 直接处理普通字节 */
                    if (priv->rx_state == UARTNET_STATE_LENGTH) {
                        /* 存储长度字节 */
                        priv->rx_length_bytes[priv->rx_length_index] = data;
                        priv->rx_length_index++;
                        
                        if (priv->rx_length_index == 2) {
                            /* 解析原始长度 */
                            u16 len = (priv->rx_length_bytes[0] << 8) | 
                                      priv->rx_length_bytes[1];
                            
                            /* 验证长度 */
                            if (len > UARTNET_MTU || len == 0) {
                                if (priv->rx_skb) {
                                    dev_kfree_skb(priv->rx_skb);
                                    priv->rx_skb = NULL;
                                }
                                priv->rx_state = UARTNET_STATE_IDLE;
                                dev->stats.rx_length_errors++;
                            } else {
                                priv->rx_frame_length = len;
                                priv->rx_skb = netdev_alloc_skb_ip_align(dev, len);
                                if (!priv->rx_skb) {
                                    priv->rx_state = UARTNET_STATE_IDLE;
                                    dev->stats.rx_dropped++;
                                } else {
                                    priv->rx_state = UARTNET_STATE_DATA;
                                }
                            }
                        }
                    } else if (priv->rx_state == UARTNET_STATE_DATA) {
                        if (priv->rx_data_received < priv->rx_frame_length) {
                            skb_put_u8(priv->rx_skb, data);
                            priv->rx_data_received++;
                        } else {
                            /* 数据溢出 */
                            dev_kfree_skb(priv->rx_skb);
                            priv->rx_skb = NULL;
                            priv->rx_state = UARTNET_STATE_IDLE;
                            dev->stats.rx_over_errors++;
                            break;
                        }
                        
                        /* 检查是否接收完所有数据 */
                        if (priv->rx_data_received >= priv->rx_frame_length) {
                            priv->rx_state = UARTNET_STATE_END;
                        }
                    }
                }
                break;
                
            case UARTNET_STATE_ESCAPE:
                /* 转义处理：异或0x20得到原始字节 */
                data = data ^ 0x20;
                priv->rx_state = priv->saved_state;
                goto restart;  // 用转义后的字节重新处理
                break;
                
            case UARTNET_STATE_END:
                if (data == FRAME_FOOTER) {
                    /* 成功接收完整帧 */
                    if (priv->rx_skb && priv->rx_skb->len == priv->rx_frame_length) {
                        priv->rx_skb->protocol = eth_type_trans(priv->rx_skb, dev);
                        napi_gro_receive(napi, priv->rx_skb);
                        // netif_receive_skb(priv->rx_skb);
                        dev->stats.rx_packets++;
                        dev->stats.rx_bytes += priv->rx_skb->len;
                        work_done++;
                    } else {
                        if (priv->rx_skb) {
                            dev_kfree_skb(priv->rx_skb);
                        }
                        dev->stats.rx_errors++;
                    }
                } else {
                    /* 帧尾错误 */
                    if (priv->rx_skb) {
                        dev_kfree_skb(priv->rx_skb);
                    }
                    dev->stats.rx_errors++;
                }
                priv->rx_skb = NULL;
                priv->rx_state = UARTNET_STATE_IDLE;
                break;
        }
    }

    /* 如果处理了所有数据，退出NAPI */
    if (work_done < budget) {
        napi_complete_done(napi, work_done);
        
        /* 重新启用RX中断 */
        writel(readl(priv->uart_base + UART011_IMSC) | UART011_RTIM | UART011_RXIM, priv->uart_base + UART011_IMSC);
    }

    return work_done;
}

/* 中断处理函数 */
static irqreturn_t uartnet_interrupt(int irq, void *dev_id)
{
    struct net_device *dev = dev_id;
    struct uartnet_priv *priv = netdev_priv(dev);
    u32 status;
    
    status = readl(priv->uart_base + UART011_MIS);
    
    /* 处理RX中断 */
    if (status & (UART011_RTIS | UART011_RXIS)) {
        priv->rx_intr++;
        /* 清除中断 */
        writel(UART011_RTIS | UART011_RXIS, priv->uart_base + UART011_ICR);
        
        /* 禁用RX中断，NAPI会重新启用 */
        writel(readl(priv->uart_base + UART011_IMSC) & ~(UART011_RTIM | UART011_RXIM), priv->uart_base + UART011_IMSC);
        UART_NET_DEBUG("uartnet_interrupt rx");
        /* 调度NAPI */
        // if (napi_schedule_prep(&priv->napi)) {
        //     __napi_schedule(&priv->napi);
        // }
        napi_schedule(&priv->napi);
    }
    
    /* 处理TX中断 */
    if (status & UART011_TXIS) {
        priv->tx_intr++;
        writel(UART011_TXIS, priv->uart_base + UART011_ICR);
        UART_NET_DEBUG("uartnet_interrupt tx");
        uartnet_tx_interrupt(dev);
    }

    if (priv->tx_intr % 100 == 0) {
        // UART_NET_DEBUG ("tx_intr = %d", priv->tx_intr);
    }

    if (priv->rx_intr % 100 == 0) {
        // UART_NET_DEBUG ("rx_intr = %d", priv->rx_intr);
    }
    
    return IRQ_HANDLED;
}

/* 网络设备打开 */
static int uartnet_open(struct net_device *dev)
{
    struct uartnet_priv *priv = netdev_priv(dev);
    int err;
    unsigned long flags;
    
    /* 重置接收状态机 */
    priv->rx_state = UARTNET_STATE_IDLE;
    priv->rx_skb = NULL;

    spin_lock_irqsave(&priv->tx_lock, flags);
    priv->tx_state = TX_STATE_IDLE;
    if (priv->tx_skb) {
        dev_kfree_skb(priv->tx_skb);
        priv->tx_skb = NULL;
    }
    spin_unlock_irqrestore(&priv->tx_lock, flags);

    /* 请求中断 (devm管理) */
    err = devm_request_irq(&dev->dev, dev->irq, uartnet_interrupt,
                           IRQF_SHARED, dev->name, dev);
    if (err) {
        dev_err(&dev->dev, "Failed to request IRQ: %d\n", err);
        return err;
    }

    /* 启用UART */
    writel(UART01x_CR_UARTEN | UART011_CR_TXE | UART011_CR_RXE, priv->uart_base + UART011_CR);
    
    /* 启用RX中断 */
    writel(UART011_RTIM | UART011_RXIM, priv->uart_base + UART011_IMSC);
    
    /* 启用NAPI */
    napi_enable(&priv->napi);
    
    netif_start_queue(dev);
    return 0;
}

/* 网络设备关闭 */
static int uartnet_stop(struct net_device *dev)
{
    struct uartnet_priv *priv = netdev_priv(dev);
    unsigned long flags;

    netif_stop_queue(dev);
    napi_disable(&priv->napi);

    /* 禁用中断 */
    writel(0, priv->uart_base + UART011_IMSC);
    
    /* 禁用UART */
    writel(0, priv->uart_base + UART011_CR);

    /* 使用的 devm_request_irq，不需要手动 free */
    // free_irq(dev->irq, dev);
    
    /* 清理接收状态 */
    if (priv->rx_skb) {
        dev_kfree_skb(priv->rx_skb);
        priv->rx_skb = NULL;
    }
    priv->rx_state = UARTNET_STATE_IDLE;

    spin_lock_irqsave(&priv->tx_lock, flags);
    if (priv->tx_skb) {
        dev_kfree_skb(priv->tx_skb);
        priv->tx_skb = NULL;
    }
    priv->tx_state = TX_STATE_IDLE;
    spin_unlock_irqrestore(&priv->tx_lock, flags);
    
    return 0;
}

static const struct net_device_ops uartnet_netdev_ops = {
    .ndo_open = uartnet_open,
    .ndo_stop = uartnet_stop,
    .ndo_start_xmit = uartnet_start_xmit,
    .ndo_validate_addr = eth_validate_addr,
    .ndo_set_mac_address = eth_mac_addr,
};

/* 探测函数 */
static int uartnet_probe(struct platform_device *pdev)
{
    struct net_device *dev;
    struct uartnet_priv *priv;
    struct resource *res;
    int err;
    
    /* 使用devm分配网络设备 */
    // dev = devm_alloc_etherdev_mqs(&pdev->dev, sizeof(struct uartnet_priv), 1, 1);
    // dev = alloc_netdev_mqs(&pdev->dev, sizeof(struct uartnet_priv), 1, 1);
    dev = alloc_netdev(0, "uartnet%d", NET_NAME_ENUM, ether_setup);
    if (!dev)
        return -ENOMEM;
    
    SET_NETDEV_DEV(dev, &pdev->dev);
    priv = netdev_priv(dev);
    priv->netdev = dev;
    
    /* 初始化自旋锁 */
    spin_lock_init(&priv->lock);
    /* 初始化接收状态机 */
    priv->rx_state = UARTNET_STATE_IDLE;
    
    spin_lock_init(&priv->tx_lock);
    spin_lock_init(&priv->tx_xmit_lock);
    priv->tx_state = TX_STATE_IDLE;
    priv->tx_skb = NULL;

    priv->tx_intr = 0;
    priv->rx_intr = 0;

    /* 获取UART资源 */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "No memory resource\n");
        err = -ENODEV;
        goto error;
    }
    
    priv->uart_base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(priv->uart_base)) {
        dev_err(&pdev->dev, "Failed to ioremap UART\n");
        err = PTR_ERR(priv->uart_base);
        goto error;
    }

    dev->irq = platform_get_irq(pdev, 0);
    if (dev->irq < 0) {
        dev_err(&pdev->dev, "Failed to get IRQ\n");
        err = priv->irq;
        goto error;
    }
    
    /* 配置UART */
    writel(0, priv->uart_base + UART011_CR); /* 禁用UART */
    writel(0x10, priv->uart_base + UART011_LCRH); /* 8N1 */
    /* 减少中断触发次数 */
    writel(UART011_IFLS_TX4_8 | UART011_IFLS_RX7_8, priv->uart_base + UART011_IFLS); /* 4/8 FIFO阈值 */
    
    /* 初始化NAPI */
    netif_napi_add(dev, &priv->napi, uartnet_poll, NAPI_WEIGHT);
    
    /* 设置网络设备 */
    dev->netdev_ops = &uartnet_netdev_ops;
    dev->mtu = UARTNET_MTU;
    dev->tx_queue_len = TX_QUEUE_LEN;
    dev->min_mtu = ETH_MIN_MTU;
    dev->max_mtu = UARTNET_MTU;
    
    /* 设置随机MAC地址 */
    eth_hw_addr_random(dev);
    
    /* 注册网络设备 */
    err = register_netdev(dev);
    if (err) {
        dev_err(&pdev->dev, "Failed to register netdev: %d\n", err);
        goto cleanup_napi;
    }
    
    platform_set_drvdata(pdev, dev);
    
    dev_info(&pdev->dev, "UART network device %s registered, IRQ %d, base %p\n",
             dev->name, priv->irq, priv->uart_base);
    return 0;

cleanup_napi:
    netif_napi_del(&priv->napi);
error:
    free_netdev(dev);
    return err;
}

/* 移除函数 */
static int uartnet_remove(struct platform_device *pdev)
{
    struct net_device *dev = platform_get_drvdata(pdev);
    struct uartnet_priv *priv = netdev_priv(dev);
    
    unregister_netdev(dev);
    netif_napi_del(&priv->napi);
    free_netdev(dev);
    return 0;
}

static struct platform_driver uartnet_driver = {
    .driver = {
        .name = DRV_NAME,
        .of_match_table = pl011_ids,
    },
    .probe = uartnet_probe,
    .remove = uartnet_remove,
};

module_platform_driver(uartnet_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("UART Network Driver with Frame Escaping and State Machine");
MODULE_VERSION("3.0");