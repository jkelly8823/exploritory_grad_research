struct hci_uart {
	struct tty_struct	*tty;
	struct serdev_device	*serdev;
	struct hci_dev		*hdev;
	unsigned long		flags;
	unsigned long		hdev_flags;

	struct work_struct	init_ready;
	struct work_struct	write_work;

	const struct hci_uart_proto *proto;
	struct percpu_rw_semaphore proto_lock;	/* Stop work for proto close */
	void			*priv;

	struct sk_buff		*tx_skb;
	unsigned long		tx_state;

	unsigned int init_speed;
	unsigned int oper_speed;

	u8			alignment;
	u8			padding;
};

/* HCI_UART proto flag bits */
#define HCI_UART_PROTO_SET	0
#define HCI_UART_REGISTERED	1
#define HCI_UART_PROTO_READY	2

/* TX states  */
#define HCI_UART_SENDING	1
#define HCI_UART_TX_WAKEUP	2

int hci_uart_register_proto(const struct hci_uart_proto *p);
int hci_uart_unregister_proto(const struct hci_uart_proto *p);
int hci_uart_register_device(struct hci_uart *hu, const struct hci_uart_proto *p);
void hci_uart_unregister_device(struct hci_uart *hu);

int hci_uart_tx_wakeup(struct hci_uart *hu);
int hci_uart_wait_until_sent(struct hci_uart *hu);
int hci_uart_init_ready(struct hci_uart *hu);
void hci_uart_init_work(struct work_struct *work);
void hci_uart_set_baudrate(struct hci_uart *hu, unsigned int speed);
bool hci_uart_has_flow_control(struct hci_uart *hu);
void hci_uart_set_flow_control(struct hci_uart *hu, bool enable);
void hci_uart_set_speeds(struct hci_uart *hu, unsigned int init_speed,
			 unsigned int oper_speed);

#ifdef CONFIG_BT_HCIUART_H4
int h4_init(void);
int h4_deinit(void);

struct h4_recv_pkt {
	u8  type;	/* Packet type */
	u8  hlen;	/* Header length */
	u8  loff;	/* Data length offset in header */
	u8  lsize;	/* Data length field size */
	u16 maxlen;	/* Max overall packet length */
	int (*recv)(struct hci_dev *hdev, struct sk_buff *skb);
};

#define H4_RECV_ACL \
	.type = HCI_ACLDATA_PKT, \
	.hlen = HCI_ACL_HDR_SIZE, \
	.loff = 2, \
	.lsize = 2, \
	.maxlen = HCI_MAX_FRAME_SIZE \

#define H4_RECV_SCO \
	.type = HCI_SCODATA_PKT, \
	.hlen = HCI_SCO_HDR_SIZE, \
	.loff = 2, \
	.lsize = 1, \
	.maxlen = HCI_MAX_SCO_SIZE

#define H4_RECV_EVENT \
	.type = HCI_EVENT_PKT, \
	.hlen = HCI_EVENT_HDR_SIZE, \
	.loff = 1, \
	.lsize = 1, \
	.maxlen = HCI_MAX_EVENT_SIZE

struct sk_buff *h4_recv_buf(struct hci_dev *hdev, struct sk_buff *skb,
			    const unsigned char *buffer, int count,
			    const struct h4_recv_pkt *pkts, int pkts_count);
#endif

#ifdef CONFIG_BT_HCIUART_BCSP
int bcsp_init(void);
int bcsp_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_LL
int ll_init(void);
int ll_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_ATH3K
int ath_init(void);
int ath_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_3WIRE
int h5_init(void);
int h5_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_INTEL
int intel_init(void);
int intel_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_BCM
int bcm_init(void);
int bcm_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_QCA
int qca_init(void);
int qca_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_AG6XX
int ag6xx_init(void);
int ag6xx_deinit(void);
#endif

#ifdef CONFIG_BT_HCIUART_MRVL
int mrvl_init(void);
int mrvl_deinit(void);
#endif