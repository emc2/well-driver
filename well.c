#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>
#include "usbdevs.h"

#define USB_DEBUG_VAR well_debug
#include <dev/usb/usb_debug.h>

#include <sys/mouse.h>

#include "logging.h"

DEFINE_LOG_SYSTEM(well, LVL_DEBUG);

#define WELL_ERROR(args...) LOG_ERROR_PREFIX(well, args)
#define WELL_WARN(args...) LOG_WARN_PREFIX(well, args)
#define WELL_MESSAGE(args...) LOG_MESSAGE_PREFIX(well, args)
#define WELL_INFO(args...) LOG_INFO_PREFIX(well, args)
#define WELL_DEBUG(args...) LOG_DEBUG_PREFIX(well, args)

#define WELL_DRIVER_NAME "well"
#define WELL_FIFO_BUF_SIZE  8 /* bytes */
#define WELL_FIFO_QUEUE_MAXLEN 50 /* units */
#define WELL_BUTTON_DATALEN 4
#define WELL_TYPE_1_OFFSET 26
#define WELL_TYPE_2_OFFSET 30
#define WELL_FINGER_SIZE 28
#define WELL_FINGER_DATALEN (WELL_FINGER_SIZE * 16)
#define WELL_MODE_LENGTH 8

/* define payload protocols */
enum {
	DEV_WELLSPRING,
	DEV_WELLSPRING2,
	DEV_WELLSPRING3,
	DEV_WELLSPRING4,
	DEV_WELLSPRING4a,
	DEV_WELLSPRING5,
	DEV_WELLSPRING5a,
	DEV_WELLSPRING6,
	DEV_WELLSPRING6a,
	DEV_WELLSPRING_N
};

enum {
        RES_PRESSURE = 256,
	RES_WIDTH = 16,
	RES_X = 1280,
	RES_Y = 800,
	NOISE_WIDTH = 1,
	NOISE_PRESSURE = 6,
	NOISE_X = 5,
	NOISE_Y = 3
};

enum {
        INTEGRATED_BUTTON = 0x1
};

enum {
	WELL_RESET,
	WELL_INTR_TRACKPAD,
	//  	WELL_INTR_BUTTON,
	WELL_N_TRANSFER,
};

enum {
        BUTTON_ENDPOINT = 0x84,
	TRACKPAD_ENDPOINT = 0x81
};

enum {
        WELL_ENABLED = 0x1
};

typedef enum interface_mode {
	RAW_SENSOR_MODE = 0x01,
	HID_MODE        = 0x08
} interface_mode;

struct well_softc {
	device_t               sc_dev;
	struct usb_device     *sc_usb_device;
	char                   sc_mode_bytes[WELL_MODE_LENGTH]; /* device mode */
	struct mtx             sc_mutex; /* for synchronization */
	struct usb_xfer       *sc_xfer[WELL_N_TRANSFER];
	struct usb_fifo_sc     sc_fifo;

	const struct well_dev_params *sc_params;

	mousehw_t              sc_hw;
	mousemode_t            sc_mode;
	u_int                  sc_pollrate;
	mousestatus_t          sc_status;
	u_int                  sc_state;
        u_int sc_errs;
};

struct well_calib {
        int res;
        int noise;
        int min;
        int max;
};

const struct well_dev_params {
        int flags;
        char* name;
        int button_endpoint;
        int trackpad_endpoint;
        u_int button_datalen;
        u_int trackpad_datalen;
        struct well_calib press_calib;
        struct well_calib width_calib;
        struct well_calib x_calib;
        struct well_calib y_calib;
} well_dev_params[DEV_WELLSPRING_N] = {
        [DEV_WELLSPRING] = {
	        .flags = 0,
		.name = "Wellspring",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_1_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 256
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4824,
			.max = 5324
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -172,
			.max = 5820
	        }
	},
	[DEV_WELLSPRING2] = {
	        .flags = 0,
		.name = "Wellspring 2",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_1_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 256
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4824,
			.max = 4824
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -172,
			.max = 4290
	        }
	},
	[DEV_WELLSPRING3] = {
 	        .flags = INTEGRATED_BUTTON,
		.name = "Wellspring 3",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_2_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 300
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4460,
			.max = 5166
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -75,
			.max = 6700
	        }
	},
	[DEV_WELLSPRING4] = {
 	        .flags = INTEGRATED_BUTTON,
		.name = "Wellspring 4",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_2_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 300
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4620,
			.max = 5140
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -150,
			.max = 6600
	        }
	},
	[DEV_WELLSPRING4a] = {
 	        .flags = INTEGRATED_BUTTON,
		.name = "Wellspring 4a",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_2_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 300
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4616,
			.max = 5112
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -142,
			.max = 5234
	        }
	},
	[DEV_WELLSPRING5] = {
 	        .flags = INTEGRATED_BUTTON,
		.name = "Wellspring 5",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_2_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 300
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4415,
			.max = 5050
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -55,
			.max = 6680
	        }
	},
	[DEV_WELLSPRING5a] = {
 	        .flags = INTEGRATED_BUTTON,
		.name = "Wellspring 5a",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_2_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 300
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4750,
			.max = 5280
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -150,
			.max = 6730
	        }
	},
	[DEV_WELLSPRING6] = {
 	        .flags = INTEGRATED_BUTTON,
		.name = "Wellspring 6",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_2_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 300
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4620,
			.max = 5140
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -150,
			.max = 6600
	        }
	},
	[DEV_WELLSPRING6a] = {
 	        .flags = INTEGRATED_BUTTON,
		.name = "Wellspring 6a",
		.button_datalen = WELL_BUTTON_DATALEN,
		.trackpad_datalen = WELL_TYPE_2_OFFSET +
		        WELL_FINGER_DATALEN,
		.press_calib = {
  	                .res = RES_PRESSURE,
			.noise = NOISE_PRESSURE,
			.min = 0,
			.max = 300
	        },
		.width_calib = {
  	                .res = RES_WIDTH,
			.noise = NOISE_WIDTH,
			.min = 0,
			.max = 2048
	        },
		.x_calib = {
  	                .res = RES_X,
			.noise = NOISE_X,
			.min = -4620,
			.max = 5140
	        },
		.y_calib = {
 	                .res = RES_Y,
			.noise = NOISE_X,
			.min = -150,
			.max = 6600
	        }
	}
};

static const STRUCT_USB_HOST_ID well_devs[] = {
	/* MacBook Air 1.1 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0223, DEV_WELLSPRING) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0224, DEV_WELLSPRING) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0225, DEV_WELLSPRING) },

	/* MacBook Pro Penryn */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0230, DEV_WELLSPRING2) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0231, DEV_WELLSPRING2) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0232, DEV_WELLSPRING2) },

	/* MacBook 5,1 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0236, DEV_WELLSPRING3) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0237, DEV_WELLSPRING3) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0238, DEV_WELLSPRING3) },

	/* MacBook Air 3.2 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x023f, DEV_WELLSPRING4) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0240, DEV_WELLSPRING4) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0241, DEV_WELLSPRING4) },

	/* MacBook Air 3.1 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0242, DEV_WELLSPRING4a) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0243, DEV_WELLSPRING4a) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0244, DEV_WELLSPRING4a) },

	/* MacBook Pro 8,2 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0252, DEV_WELLSPRING5a) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0253, DEV_WELLSPRING5a) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0254, DEV_WELLSPRING5a) },

	/* MacBook Pro 8,1 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0245, DEV_WELLSPRING5) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0246, DEV_WELLSPRING5) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x0247, DEV_WELLSPRING5) },

	/* MacBook Air 4.2 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x024c, DEV_WELLSPRING6a) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x024d, DEV_WELLSPRING6a) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x024e, DEV_WELLSPRING6a) },

	/* MacBook Air 4.1 */
	{ USB_VPI(USB_VENDOR_APPLE, 0x0249, DEV_WELLSPRING6) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x024a, DEV_WELLSPRING6) },
	{ USB_VPI(USB_VENDOR_APPLE, 0x024b, DEV_WELLSPRING6) },

};

/* Device methods. */
static device_probe_t well_probe;
static device_attach_t well_attach;
static device_detach_t well_detach;
static usb_callback_t well_trackpad_intr;
static usb_callback_t well_button_intr;
static usb_callback_t well_reset_callback;

static const struct usb_config well_config[WELL_N_TRANSFER] = {
  /*
	[WELL_INTR_BUTTON] = {
		.type      = UE_INTERRUPT,
		.endpoint  = BUTTON_ENDPOINT,
		.direction = UE_DIR_IN,
		.flags = {
			.pipe_bof = 1,
			.short_xfer_ok = 1,
		},
		.bufsize   = 0,
		.callback  = &well_button_intr,
	},
  */
	[WELL_INTR_TRACKPAD] = {
		.type      = UE_INTERRUPT,
		.endpoint  = TRACKPAD_ENDPOINT,
		.direction = UE_DIR_IN,
		//		.if_index = 1,
		.flags = {
			.pipe_bof = 1,
			.short_xfer_ok = 1,
		},
		.bufsize   = 0,
		.callback  = &well_trackpad_intr,
	},
	[WELL_RESET] = {
		.type      = UE_CONTROL,
		.endpoint  = 0, /* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = sizeof(struct usb_device_request) + WELL_MODE_LENGTH,
		.callback  = &well_reset_callback,
		.interval = 0,  /* no pre-delay */
	},
};

static usb_fifo_cmd_t   well_start_read;
static usb_fifo_cmd_t   well_stop_read;
static usb_fifo_open_t  well_open;
static usb_fifo_close_t well_close;
static usb_fifo_ioctl_t well_ioctl;

static struct usb_fifo_methods well_fifo_methods = {
	.f_open       = &well_open,
	.f_close      = &well_close,
	.f_ioctl      = &well_ioctl,
	.f_start_read = &well_start_read,
	.f_stop_read  = &well_stop_read,
	.basename[0]  = WELL_DRIVER_NAME,
};

static int
well_enable(struct well_softc *sc)
{
	sc->sc_state |= WELL_ENABLED;
        return 0;
}

static void
well_disable(struct well_softc *sc)
{
        sc->sc_state &= ~WELL_ENABLED;
}


usb_error_t
well_req_get_report(struct usb_device *udev, void *data)
{
	struct usb_device_request req;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_REPORT;
	USETW(req.wValue, 0x300);
	USETW(req.wIndex, 0);
	USETW(req.wLength, WELL_MODE_LENGTH);

	return (usbd_do_request(udev, NULL /* mutex */, &req, data));
}

static int
well_set_device_mode(struct well_softc *sc, interface_mode mode)
{
	usb_device_request_t  req;
	usb_error_t           err;

	if ((mode != RAW_SENSOR_MODE) && (mode != HID_MODE))
		return (ENXIO);

	sc->sc_mode_bytes[0] = mode;
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW(req.wValue, 0x300);
	USETW(req.wIndex, 0);
	USETW(req.wLength, WELL_MODE_LENGTH);
	err = usbd_do_request(sc->sc_usb_device, NULL, &req, sc->sc_mode_bytes);
	if (err != USB_ERR_NORMAL_COMPLETION)
		return (ENXIO);

	return (0);
}

/* Taken from the atp driver: set the mode to RAW_SENSOR to get
 * complete info.
 */
static int
well_set_mode(struct well_softc *sc, interface_mode mode)
{
	usb_error_t err;

	memset(sc->sc_mode_bytes, 0, WELL_MODE_LENGTH);

	WELL_DEBUG("reading sensor mode\n");
	err = well_req_get_report(sc->sc_usb_device, sc->sc_mode_bytes);
	if (err != USB_ERR_NORMAL_COMPLETION) {
		WELL_ERROR("failed to read device mode (%d)\n", err);
		return (ENXIO);
	}

	WELL_DEBUG("sensor mode is %llx\n", *((unsigned long long*)&(sc->sc_mode_bytes)));
	WELL_DEBUG("setting to raw sensor mode\n");

	if (well_set_device_mode(sc, mode) != 0) {
		WELL_ERROR("failed to set mode to 'RAW_SENSOR' (%d)\n", err);
		return (ENXIO);
	}

	return 0;
}

static int
well_open(struct usb_fifo *fifo, int fflags)
{
        WELL_DEBUG("open message\n");
	int out = 0;

        if(fflags & FREAD) {
		struct well_softc *sc = usb_fifo_softc(fifo);
		int err;

		if(sc->sc_state & WELL_ENABLED)
		        return EBUSY;

		if ((err = usb_fifo_alloc_buffer(fifo,
		        WELL_FIFO_BUF_SIZE, WELL_FIFO_QUEUE_MAXLEN))) {
		        WELL_ERROR("failed to allocate fifo buffer (%d)\n", err);
			return (ENOMEM);
		}

		out = well_enable(sc);
        }
        return 0;
}

static void
well_close(struct usb_fifo *fifo, int fflags)
{
        WELL_DEBUG("close message\n");
	if (fflags & FREAD) {
		struct well_softc *sc = usb_fifo_softc(fifo);

		well_disable(sc);
		usb_fifo_free_buffer(fifo);
	}
}

static void
well_start_read(struct usb_fifo *fifo)
{
  WELL_DEBUG("start read message\n");
	struct well_softc *sc = usb_fifo_softc(fifo);
	int rate;

	/* Check if we should override the default polling interval */
	rate = sc->sc_pollrate;
	/* Range check rate */
	if (rate > 1000)
		rate = 1000;
	/* Check for set rate */
	if ((rate > 0) && (sc->sc_xfer[WELL_INTR_TRACKPAD] != NULL)) {
		/* Stop current transfer, if any */
		usbd_transfer_stop(sc->sc_xfer[WELL_INTR_TRACKPAD]);
		/* Set new interval */
		usbd_xfer_set_interval(sc->sc_xfer[WELL_INTR_TRACKPAD], 1000 / rate);
		/* Only set pollrate once */
		sc->sc_pollrate = 0;
		WELL_DEBUG("set tranfer rate to %d\n", rate);
	}

	well_set_mode(sc, RAW_SENSOR_MODE);
	usbd_transfer_start(sc->sc_xfer[WELL_INTR_TRACKPAD]);
	WELL_DEBUG("starting transfer\n");
}

static void
well_stop_read(struct usb_fifo *fifo)
{
  WELL_DEBUG("stop read message\n");
	struct well_softc *sc = usb_fifo_softc(fifo);

	well_set_mode(sc, HID_MODE);
	usbd_transfer_stop(sc->sc_xfer[WELL_INTR_TRACKPAD]);
}

int
well_ioctl(struct usb_fifo *fifo, u_long cmd, void *addr, int fflags)
{
        return 0;
}

static void
well_button_intr(struct usb_xfer *xfer, usb_error_t error)
{
        int len;
	char data[WELL_BUTTON_DATALEN];
	struct well_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
                WELL_DEBUG("transferred interrupt\n");

		if (len > sc->sc_params->button_datalen) {
		        WELL_WARN(
			    "truncating large packet from %u to %u bytes\n",
			    len, sc->sc_params->button_datalen);
			len = sc->sc_params->button_datalen;
		}

		if (len < sc->sc_params->button_datalen) {
		        WELL_WARN("received short packet, ignoring\n");
			goto tr_setup;
		}

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, data,
		    sc->sc_params->button_datalen);
		WELL_DEBUG("got data { %x, %x, %x, %x }\n",
			   data[0], data[1], data[2], data[3]);
		// FALLTHROUGH
	case USB_ST_SETUP:
                WELL_DEBUG("setting up transfer\n");
	tr_setup:
		/* check if we can put more data into the FIFO */
		if (usb_fifo_put_bytes_max(
			    sc->sc_fifo.fp[USB_FIFO_RX]) != 0) {
			usbd_xfer_set_frame_len(xfer, 0,
			    sc->sc_params->button_datalen);
			usbd_transfer_submit(xfer);
		}
		break;

	default:                        /* Error */
	  WELL_DEBUG("error interrupt (%d)\n", error);
		if (error != USB_ERR_CANCELLED) {
			/* try clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}

}

static void
well_trackpad_intr(struct usb_xfer *xfer, usb_error_t error)
{
	struct well_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int len;
	char data[WELL_TYPE_2_OFFSET + WELL_FINGER_DATALEN];

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	        WELL_DEBUG("transfer complete\n");

		if (len > sc->sc_params->trackpad_datalen) {
		        WELL_WARN(
			    "truncating large packet from %u to %u bytes\n",
			    len, sc->sc_params->trackpad_datalen);
			len = sc->sc_params->trackpad_datalen;
		}

		if (len < sc->sc_params->trackpad_datalen) {
		  sc->sc_errs++;
		        WELL_WARN("received short packet, ignoring\n");
			goto tr_setup;
		}

		sc->sc_errs = 0;
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, data,
		    sc->sc_params->trackpad_datalen);
		for(unsigned int i = 0; i < WELL_TYPE_2_OFFSET + WELL_FINGER_DATALEN; i++)
		  WELL_DEBUG("data[%d] = %x\n", i, data[i]);

	  // FALLTHROUGH
	case USB_ST_SETUP:
	tr_setup:
                WELL_DEBUG("setting up transfer\n");
		if (sc->sc_errs < 5) {
		/* check if we can put more data into the FIFO */
		if (usb_fifo_put_bytes_max(
			    sc->sc_fifo.fp[USB_FIFO_RX]) != 0) {
			usbd_xfer_set_frame_len(xfer, 0,
			    sc->sc_params->trackpad_datalen);
			usbd_transfer_submit(xfer);
		}} else {
		  WELL_ERROR("Too many errors, stopping\n");
		}
		break;

	default:                        /* Error */
	  WELL_DEBUG("error interrupt (%s)\n", usbd_errstr(error));
	  sc->sc_errs++;
		if (error != USB_ERR_CANCELLED) {
			/* try clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

void
well_reset_callback(struct usb_xfer *xfer, usb_error_t error)
{

	usb_device_request_t req;
	struct usb_page_cache *pc;
	struct well_softc *sc = usbd_xfer_softc(xfer);

	WELL_DEBUG("reset message received\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
		sc->sc_mode_bytes[0] = RAW_SENSOR_MODE;
		req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
		req.bRequest = UR_SET_REPORT;
		USETW2(req.wValue,
		    (uint8_t)0x03 /* type */, (uint8_t)0x00 /* id */);
		USETW(req.wIndex, 0);
		USETW(req.wLength, WELL_MODE_LENGTH);

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_in(pc, 0, &req, sizeof(req));
		pc = usbd_xfer_get_frame(xfer, 1);
		usbd_copy_in(pc, 0, sc->sc_mode_bytes, WELL_MODE_LENGTH);

		usbd_xfer_set_frame_len(xfer, 0, sizeof(req));
		usbd_xfer_set_frame_len(xfer, 1, WELL_MODE_LENGTH);
		usbd_xfer_set_frames(xfer, 2);
		usbd_transfer_submit(xfer);
		break;
	case USB_ST_TRANSFERRED:
	default:
		break;
	}

}


static int
well_probe(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	WELL_INFO("probing\n");
	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if ((uaa->info.bInterfaceClass != UICLASS_HID) ||
	    (uaa->info.bInterfaceProtocol != 0))
		return (ENXIO);

	WELL_DEBUG("bInterfaceProtocol = %d, bIfaceIndex = %d\n",
	       uaa->info.bInterfaceProtocol,
	       uaa->info.bIfaceIndex);

	const int out = usbd_lookup_id_by_uaa(well_devs, sizeof(well_devs), uaa);

	WELL_DEBUG("probing result: %x\n", out);

	return out;

}

#include <sys/sx.h>
#include <sys/condvar.h>
#include <dev/usb/usb_device.h>

static int
well_attach(device_t dev)
{
	struct well_softc      *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	usb_error_t            err;

	WELL_INFO("attaching...\n");
	sc->sc_dev        = dev;
	sc->sc_usb_device = uaa->device;

	mtx_init(&sc->sc_mutex, "wellmtx", NULL, MTX_DEF | MTX_RECURSE);

	WELL_DEBUG("%d endpoints:\n", sc->sc_usb_device->endpoints_max);
	for(unsigned int i = 0; i < sc->sc_usb_device->endpoints_max; i++) {
	        struct usb_endpoint *ep = sc->sc_usb_device->endpoints + i;

		WELL_DEBUG("endpoint %d bLength: %u\n", i, ep->edesc->bLength);
		WELL_DEBUG("endpoint %d bDescriptorType: %u\n", i, ep->edesc->bDescriptorType);
		WELL_DEBUG("endpoint %d bEndpointAddress: %x\n", i, ep->edesc->bEndpointAddress);
		WELL_DEBUG("endpoint %d bmAttributes: %x\n", i, ep->edesc->bmAttributes);
		WELL_DEBUG("endpoint %d wMaxPacketSize: %u\n", i, ep->edesc->wMaxPacketSize);
		WELL_DEBUG("endpoint %d bInterval: %u\n", i, ep->edesc->bInterval);
		WELL_DEBUG("endpoint %d unused: %x\n", i, ep->unused);
		WELL_DEBUG("endpoint %d methods: %x\n", i, ep->methods);
		WELL_DEBUG("endpoint %d iface_index: %x\n", i, ep->iface_index);
		WELL_DEBUG("endpoint %d usb_smask: %x\n", i, ep->usb_smask);
		WELL_DEBUG("endpoint %d usb_cmask: %x\n", i, ep->usb_cmask);
		WELL_DEBUG("endpoint %d usb_uframe: %x\n", i, ep->usb_uframe);

	}

	//	WELL_DEBUG("usbd_get_endpoint(WELL_BUTTON_INTR) = %p\n",
	//	   usbd_get_endpoint(sc->sc_usb_device, ));

	if(err != 0) {
	        WELL_ERROR("mode reset failed (%d)\n", err);
		return err;
	}

	/* Now setup the transfers */
	WELL_DEBUG("initializing USB transfer\n");
	err = usbd_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, well_config,
	    WELL_N_TRANSFER, sc, &sc->sc_mutex);

	if (err) {
		WELL_ERROR("cannot initialize USB transfer: %s\n",
			   usbd_errstr(err));
		goto detach;
	}
	WELL_INFO("initializing FIFO\n");

	if (usb_fifo_attach(sc->sc_usb_device, sc, &sc->sc_mutex,
		&well_fifo_methods, &sc->sc_fifo,
		device_get_unit(dev), 0 - 1, uaa->info.bIfaceIndex,
		UID_ROOT, GID_OPERATOR, 0644)) {
		WELL_ERROR("cannot attach USB fifo: %s\n",
			   usbd_errstr(err));
		goto detach;
	}

	/* Now initialize the outbound interface */
	device_set_usb_desc(dev);
	sc->sc_params = &well_dev_params[uaa->driver_info];
	WELL_INFO("device version is %s\n", well_dev_params[uaa->driver_info].name);
	sc->sc_hw.buttons       = 3;
	sc->sc_hw.iftype        = MOUSE_IF_USB;
	sc->sc_hw.type          = MOUSE_PAD;
	sc->sc_hw.model         = MOUSE_MODEL_GENERIC;
	sc->sc_hw.hwid          = 0;
	sc->sc_mode.protocol    = MOUSE_PROTO_MSC;
	sc->sc_mode.rate        = -1;
	sc->sc_mode.resolution  = MOUSE_RES_UNKNOWN;
	sc->sc_mode.accelfactor = 0;
	sc->sc_mode.level       = 0;
	sc->sc_mode.packetsize  = MOUSE_MSC_PACKETSIZE;
	sc->sc_mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
	sc->sc_mode.syncmask[1] = MOUSE_MSC_SYNC;
	sc->sc_state            = 0;
	sc->sc_errs = 0;
#if 0
	sc->sc_left_margin  = atp_mickeys_scale_factor;
	sc->sc_right_margin = (sc->sc_params->n_xsensors - 1) *
		atp_mickeys_scale_factor;

#endif
	return (0);

detach:
	well_detach(dev);
	return (ENOMEM);
}


static int
well_detach(device_t dev)
{
	struct well_softc *sc = device_get_softc(dev);

	WELL_INFO("detaching...\n");

	if (sc->sc_state & WELL_ENABLED) {
		mtx_lock(&sc->sc_mutex);
		well_disable(sc);
		mtx_unlock(&sc->sc_mutex);
	}

	usb_fifo_detach(&sc->sc_fifo);
	usbd_transfer_unsetup(sc->sc_xfer, WELL_N_TRANSFER);
	mtx_destroy(&sc->sc_mutex);
	WELL_INFO("detached...\n");

	return (0);
}

static device_method_t well_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,  well_probe),
	DEVMETHOD(device_attach, well_attach),
	DEVMETHOD(device_detach, well_detach),
	{ 0, 0 }
};

static driver_t well_driver = {
	WELL_DRIVER_NAME,
	well_methods,
	sizeof(struct well_softc)
};

static devclass_t well_devclass;

DRIVER_MODULE(well, uhub, well_driver, well_devclass, NULL, 0);
MODULE_DEPEND(well, usb, 1, 1, 1);
MODULE_VERSION(well, 1);
