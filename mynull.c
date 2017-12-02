#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/devops.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

/* ==============================================================
   Function prototypes
   ============================================================== */

static int mynull_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int mynull_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int mynull_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd,
			  void *arg, void **resultp);

static int mynull_open(dev_t *devp, int flags, int otyp,
		       cred_t *cred_p);
static int mynull_close(dev_t dev, int flag, int otyp, cred_t *cred_p);
static int mynull_read(dev_t dev, struct uio *uio_p, cred_t *cred_p);
static int mynull_write(dev_t dev, struct uio *uio_p, cred_t *cred_p);


/* ==============================================================
   State structure type definition
   ============================================================== */

typedef struct mynull_soft {
    dev_info_t	*dip;
    int32_t	instance;
} mynull_soft_t;


/* ==============================================================
   Static driver-global variables
   ============================================================== */

static void	*mynull_soft_root;
static kmutex_t	mynull_global_lock;


static struct cb_ops mynull_cb_ops = {
    mynull_open,        /* (*cb_open)() */
    mynull_close,       /* (*cb_close)() */
    nodev,              /* (*cb_strategy)() */
    nodev,              /* (*cb_print)() */
    nodev,              /* (*cb_dump)() */
    mynull_read,        /* (*cb_read)() */
    mynull_write,       /* (*cb_write)() */
    nodev,              /* (*cb_ioctl)() */
    nodev,              /* (*cb_devmap)() */
    nodev,              /* (*cb_mmap)() */
    nodev,              /* (*cb_segmap)() */
    nochpoll,           /* (*cb_chpoll)() */
    ddi_prop_op,        /* (*cb_prop_op)() */
    NULL,               /* *cb_str */
    D_MP,               /* cb_flag */
    CB_REV,             /* cb_rev */
    nodev,              /* (*cb_aread)() */
    nodev               /* (*cb_awrite)() */
};


static struct dev_ops mynull_dev_ops = {
    DEVO_REV,           /* devo_rev */
    0,                  /* devo_refcnt */
    mynull_getinfo,     /* (*devo_getinfo)() */
    nulldev,            /* (*devo_identify)() */
    nulldev,            /* (*devo_probe)() */
    mynull_attach,      /* (*devo_attach)() */
    mynull_detach,      /* (*devo_detach)() */
    nodev,              /* (*devo_reset)() */
    &mynull_cb_ops,     /* *devo_cb_ops */
    NULL,               /* *devo_bus_ops */
    NULL                /* (*devo_power)() */
};


static struct modldrv mynull_modldrv = {
    &mod_driverops,
    "mynull 1.0",
    &mynull_dev_ops
};


static struct modlinkage mynull_modlinkage = {
    MODREV_1,
    { &mynull_modldrv, NULL }
};



/* ==============================================================
   Driver initialization, removal, and information
   ============================================================== */

int _init()
{
    int status;

    /* Initialization of driver-global variables */
    status = ddi_soft_state_init(&mynull_soft_root, sizeof(mynull_soft_t), 1);
    if (status != 0)
	return(status);

    mutex_init(&mynull_global_lock, NULL, MUTEX_DRIVER, NULL);

    status = mod_install(&mynull_modlinkage);
    if (status != 0) {
        /* mod_install() failed -- undo initialization */
	mutex_destroy(&mynull_global_lock);
	ddi_soft_state_fini(&mynull_soft_root);
    }

    return(status);
}


int _fini()
{
    int status;

    status = mod_remove(&mynull_modlinkage);
    if (status == 0) {
	/* mod_remove() success -- clean-up global resources */
	mutex_destroy(&mynull_global_lock);
	ddi_soft_state_fini(&mynull_soft_root);
    }

    return(status);
}


int _info(struct modinfo *modinfop)
{
    return(mod_info(&mynull_modlinkage, modinfop));
}



/* ==============================================================
   Autoconfiguration Support Functions
   ============================================================== */

static int mynull_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
    mynull_soft_t *msp;
    int32_t instance;

    if (cmd != DDI_ATTACH)
	return(DDI_FAILURE);

    mutex_enter(&mynull_global_lock);

    instance = ddi_get_instance(dip);
    if (ddi_soft_state_zalloc(mynull_soft_root, instance) != DDI_SUCCESS) {
	mutex_exit(&mynull_global_lock);
	return(DDI_FAILURE);
    }

    msp = (mynull_soft_t *)ddi_get_soft_state(mynull_soft_root, instance);
    if (msp == NULL) {
	cmn_err(CE_WARN,
		"mynull: Instance %d failed to obtain soft state structure!",
		instance);
	ddi_soft_state_free(mynull_soft_root, instance);
	mutex_exit(&mynull_global_lock);
	return(DDI_FAILURE);
    }

    msp->dip = dip;
    msp->instance = instance;

    if (ddi_create_minor_node(dip, "mynull", S_IFCHR, instance,
			      DDI_PSEUDO, 0) != DDI_SUCCESS) {
	cmn_err(CE_NOTE,
		"mynull: Instance %d failed to create minor node.\n",
		instance);
	ddi_soft_state_free(mynull_soft_root, instance);
	mutex_exit(&mynull_global_lock);
	return(DDI_FAILURE);
    }

    mutex_exit(&mynull_global_lock);

    return(DDI_SUCCESS);
}


static int mynull_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
    mynull_soft_t *msp;
    int32_t instance;

    if (cmd != DDI_DETACH)
	return(DDI_FAILURE);

    mutex_enter(&mynull_global_lock);

    instance = ddi_get_instance(dip);

    msp = (mynull_soft_t *)ddi_get_soft_state(mynull_soft_root, instance);
    if (msp == NULL) {
	mutex_exit(&mynull_global_lock);
	return(DDI_FAILURE);
    }

    ddi_remove_minor_node(dip, NULL);
    ddi_soft_state_free(mynull_soft_root, instance);

    mutex_exit(&mynull_global_lock);

    return(DDI_SUCCESS);
}


static int mynull_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd,
			  void *arg, void **resultp)
{
    mynull_soft_t *msp;
    int32_t retval;
    int32_t instance = getminor((dev_t)arg);

    msp = (mynull_soft_t *)ddi_get_soft_state(mynull_soft_root, instance);
    if (msp == NULL) {
	*resultp = NULL;
	return(DDI_FAILURE);
    }

    switch (cmd) {
    case DDI_INFO_DEVT2DEVINFO:
	*resultp = (void *)msp->dip;
	retval = DDI_SUCCESS;
	break;

    case DDI_INFO_DEVT2INSTANCE:
	*resultp = (void *)msp->instance;
	retval = DDI_SUCCESS;
	break;

    default:
	*resultp = NULL;
	retval = DDI_FAILURE;
	break;
    }

    return(retval);
}



/* ==============================================================
   Device entry points
   ============================================================== */

static int mynull_open(dev_t *devp, int flags, int otyp,
		       cred_t *cred_p)
{
    mynull_soft_t *msp;
    int32_t instance = getminor(*devp);

    msp = (mynull_soft_t *)ddi_get_soft_state(mynull_soft_root, instance);
    if (msp == NULL)
	return(ENXIO);

    return(0);
}


static int mynull_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
    mynull_soft_t *msp;
    int32_t instance = getminor(dev);

    msp = (mynull_soft_t *)ddi_get_soft_state(mynull_soft_root, instance);
    if (msp == NULL)
	return(ENXIO);

    return(0);
}


static int mynull_read(dev_t dev, struct uio *uio_p, cred_t *cred_p)
{
    mynull_soft_t *msp;
    int32_t instance = getminor(dev);

    msp = (mynull_soft_t *)ddi_get_soft_state(mynull_soft_root, instance);
    if (msp == NULL)
	return(ENXIO);

    return(0);
}


#define BUFLEN	1024

static int mynull_write(dev_t dev, struct uio *uio_p, cred_t *cred_p)
{
    mynull_soft_t *msp;
    int32_t instance = getminor(dev);
    uint8_t buf[BUFLEN];
    int status = 0;

    msp = (mynull_soft_t *)ddi_get_soft_state(mynull_soft_root, instance);
    if (msp == NULL)
	return(ENXIO);

    while (uio_p->uio_resid > 0) {
	if (uio_p->uio_resid >= BUFLEN)
	    status = uiomove(buf, BUFLEN, UIO_WRITE, uio_p);
	else
	    status = uiomove(buf, uio_p->uio_resid, UIO_WRITE, uio_p);

	if (status != 0)
	    break;
    }

    return(status);
}
