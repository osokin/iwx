struct iwx_node {
	struct ieee80211_node	in_node;
	int			in_station;
#define IWX_MAX_IBSSNODE	32
};

struct iwx_softc {
	struct mtx		sc_mtx;
	struct ieee80211com	sc_ic;
	struct mbufq		sc_snd;
	device_t		sc_dev;

	void			(*sc_node_free)(struct ieee80211_node *);

	uint8_t			sc_mcast[IEEE80211_ADDR_LEN];
	struct unrhdr		*sc_unr;

	uint32_t		flags;
	uint32_t		fw_state;

	struct resource		*sc_irq;
	struct resource		*sc_mem;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	bus_dma_tag_t		sc_dmat;
	void			*sc_ih;

	struct callout		sc_wdtimer;
	struct callout		sc_rftimer;

	int			sc_tx_timer;
	int			sc_state_timer;
	int			sc_busy_timer;
};
