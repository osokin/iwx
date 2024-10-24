/*
 * Simple KLD to play with the PCI functions.
 *
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_input.h>
#include <net80211/ieee80211_regdomain.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <dev/iwx/if_iwxvar.h>

MODULE_DEPEND(iwx, pci,  1, 1, 1);
MODULE_DEPEND(iwx, wlan, 1, 1, 1);
MODULE_DEPEND(iwx, firmware, 1, 1, 1);

struct iwx_ident {
	uint16_t	vendor;
	uint16_t	device;
	const char	*name;
};

static const struct iwx_ident iwx_ident_table[] = {
	{ 0x8086, 0xa0f0, "Intel(R) Wi-Fi 6 AX201" },

	{ 0, 0, NULL }
};

static int iwx_probe(device_t);
static int iwx_attach(device_t);
static void	iwx_intr(void *);
/*
static int iwx_detach(device_t);
static int iwx_shutdown(device_t);
static int iwx_suspend(device_t);
static int iwx_resume(device_t);

static void	iwx_stop(struct iwx_softc *);
*/

static device_method_t iwx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		iwx_probe),
	DEVMETHOD(device_attach,	iwx_attach),
/*
	DEVMETHOD(device_detach,	iwx_detach),
	DEVMETHOD(device_shutdown,	iwx_shutdown),
	DEVMETHOD(device_suspend,	iwx_suspend),
	DEVMETHOD(device_resume,	iwx_resume),
*/
	DEVMETHOD_END
};

static driver_t iwx_driver = {
	"iwx",
	iwx_methods
};

DRIVER_MODULE(iwx, pci, iwx_driver, NULL, NULL);

MODULE_VERSION(iwx, 1);

/*
 * % pciconf -l iwlwifi0
 * iwlwifi0@pci0:0:20:3:   class=0x028000 rev=0x20 hdr=0x00 vendor=0x8086 device=0xa0f0 subvendor=0x8086 subdevice=0x0070
 */

static int
iwx_probe(device_t dev)
{
	const struct iwx_ident *ident;

	for (ident = iwx_ident_table; ident->name != NULL; ident++) {
		if (pci_get_vendor(dev) == ident->vendor &&
			pci_get_device(dev) == ident->device) {
				device_set_desc(dev, ident->name);
				return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

#define PCI_CFG_RETRY_TIMEOUT	0x041

static int
iwx_attach(device_t dev)
{
	struct iwx_softc *sc;
	int count, error, rid;
	uint16_t reg;

	sc = device_get_softc(dev);

	printf("iwx(4) attach for : deviceID : 0x%x\n", pci_get_devid(dev));

	pci_write_config(dev, PCI_CFG_RETRY_TIMEOUT, 0x00, 1);
	pci_enable_busmaster(dev);
	reg = pci_read_config(dev, PCIR_STATUS, sizeof(reg));
	if (reg & PCIM_STATUS_INTxSTATE) {
		reg &= ~PCIM_STATUS_INTxSTATE;
	}
	pci_write_config(dev, PCIR_STATUS, reg, sizeof(reg));

	rid = PCIR_BAR(0);
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
		RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(sc->sc_dev, "can't map mem space\n");
		return (ENXIO);
	}

	sc->sc_st = rman_get_bustag(sc->sc_mem);
	sc->sc_sh = rman_get_bushandle(sc->sc_mem);

	/* Install interrupt handler. */
	count = 1;
	rid = 0;
	if (pci_alloc_msi(dev, &count) == 0)
		rid = 1;
	sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE |
	    (rid != 0 ? 0 : RF_SHAREABLE));
	if (sc->sc_irq == NULL) {
		device_printf(dev, "can't map interrupt\n");
			return (ENXIO);
	}
	error = bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, iwx_intr, sc, &sc->sc_ih);
	if (error != 0) {
		device_printf(dev, "can't establish interrupt");
		return (error);
	}
	sc->sc_dmat = bus_get_dma_tag(sc->sc_dev);

	return 0;
}

/*
static int
iwx_detach(device_t dev)
{
	struct iwx_softc *sc = device_get_softc(dev);

	if (sc->sc_irq != NULL) {
		bus_teardown_intr(dev, sc->sc_irq, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ,
		rman_get_rid(sc->sc_irq), sc->sc_irq);
		pci_release_msi(dev);
	}
	if (sc->sc_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
			rman_get_rid(sc->sc_mem), sc->sc_mem);

	printf("iwx(4) detach!\n");

	return 0;
}

static int
iwx_shutdown(device_t dev)
{
	struct iwx_softc *sc;

	sc = device_get_softc(dev);
	iwx_stop(sc);

	return 0;
}

static int
iwx_suspend(device_t dev)
{
	struct iwx_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;

	ieee80211_suspend_all(ic);
	return 0;
}

static int
iwx_resume(device_t dev)
{
	struct iwx_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;

	pci_write_config(dev, 0x41, 0, 1);

	ieee80211_resume_all(ic);
	return 0;
}

static void
iwx_stop(struct iwx_softc *sc)
{

}
*/

static void
iwx_intr(void *arg) {

	return;
}
