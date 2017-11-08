/*******************************************************************************
*   Ledger Blue - Bitcoin Wallet
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "os.h"
#include "cx.h"

#include "os_io_seproxyhal.h"
#include "string.h"

#include "btchip_internal.h"

#include "btchip_bagl_extensions.h"

#include "segwit_addr.h"

#include "glyphs.h"

#define __NAME3(a, b, c) a##b##c
#define NAME3(a, b, c) __NAME3(a, b, c)

#ifdef HAVE_U2F

#include "u2f_service.h"
#include "u2f_transport.h"

volatile unsigned char u2fMessageBuffer[U2F_MAX_MESSAGE_SIZE];

extern void USB_power_U2F(unsigned char enabled, unsigned char fido);
extern bool fidoActivated;
volatile uint8_t fidoTransport;

#endif

bagl_element_t tmp_element;

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

#define BAGL_FONT_OPEN_SANS_LIGHT_16_22PX_AVG_WIDTH 10
#define BAGL_FONT_OPEN_SANS_REGULAR_10_13PX_AVG_WIDTH 8
#define MAX_CHAR_PER_LINE 25

#define COLOR_BG_1 0xF9F9F9
#define COLOR_APP COLOR_HDR      // bitcoin 0xFCB653
#define COLOR_APP_LIGHT COLOR_DB // bitcoin 0xFEDBA9

union {
    struct {
        // char addressSummary[40]; // beginning of the output address ... end
        // of

        char fullAddress[43]; // the address
        char fullAmount[20];  // full amount
        char feesAmount[20];  // fees
    } tmp;
} vars;

unsigned int io_seproxyhal_touch_verify_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_verify_ok(const bagl_element_t *e);
unsigned int
io_seproxyhal_touch_message_signature_verify_cancel(const bagl_element_t *e);
unsigned int
io_seproxyhal_touch_message_signature_verify_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_display_cancel(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_display_ok(const bagl_element_t *e);
unsigned int io_seproxyhal_touch_exit(const bagl_element_t *e);
void ui_idle(void);

ux_state_t ux;

// display stepped screens
unsigned int ux_step;
unsigned int ux_step_count;

#ifdef HAVE_U2F
volatile u2f_service_t u2fService;

void u2f_proxy_response(u2f_service_t *service, unsigned int tx) {
    os_memset(service->messageBuffer, 0, 5);
    os_memmove(service->messageBuffer + 5, G_io_apdu_buffer, tx);
    service->messageBuffer[tx + 5] = 0x90;
    service->messageBuffer[tx + 6] = 0x00;
    u2f_send_fragmented_response(service, U2F_CMD_MSG, service->messageBuffer,
                                 tx + 7, true);
}

#endif
const bagl_element_t *ui_menu_item_out_over(const bagl_element_t *e) {
    // the selection rectangle is after the none|touchable
    e = (const bagl_element_t *)(((unsigned int)e) + sizeof(bagl_element_t));
    return e;
}

const ux_menu_entry_t menu_main[];

const ux_menu_entry_t menu_main[] = {
    {NULL, NULL, 0, NULL, "Waiting for", "signature", 0, 0},
    {NULL, os_sched_exit, 0, &C_nanos_icon_dashboard, "Quit app", NULL, 50, 29},
    UX_MENU_END};

const bagl_element_t ui_display_address_nanos[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    //{{BAGL_ICON                           , 0x01,  21,   9,  14,  14, 0, 0, 0
    //, 0xFFFFFF, 0x000000, 0, BAGL_GLYPH_ICON_TRANSACTION_BADGE  }, NULL, 0, 0,
    //0, NULL, NULL, NULL },
    {{BAGL_LABELINE, 0x01, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Confirm",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "address",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 12, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Address",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    // Hax, avoid wasting space
    {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
     G_io_apdu_buffer + 199,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    //{{BAGL_ICON                           , 0x00,   1,  1,   32,  32, 0, 0, 0
    //, 0xFFFFFF, 0x000000, 0, 0 }, &vars.tmpqr.icon_details, 0, 0, 0, NULL,
    //NULL, NULL },
};

unsigned int ui_display_address_nanos_prepro(const bagl_element_t *element) {
    if (element->component.userid > 0) {
        unsigned int display = (ux_step == element->component.userid - 1);
        if (display) {
            switch (element->component.userid) {
            case 1:
                UX_CALLBACK_SET_INTERVAL(2000);
                break;
            case 2:
                UX_CALLBACK_SET_INTERVAL(MAX(
                    3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            }
        }
        return display;
    }
    return 1;
}

unsigned int ui_display_address_nanos_button(unsigned int button_mask,
                                             unsigned int button_mask_counter);

const bagl_element_t ui_verify_message_signature_nanos[] = {
    // type                               userid    x    y   w    h  str rad
    // fill      fg        bg      fid iid  txt   touchparams...       ]
    {{BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000, 0xFFFFFF,
      0, 0},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CROSS},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
      BAGL_GLYPH_ICON_CHECK},
     NULL,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x01, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "Confirm",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
    {{BAGL_LABELINE, 0x01, 0, 26, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "signature",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

    {{BAGL_LABELINE, 0x02, 0, 12, 128, 32, 0, 0, 0, 0xFFFFFF, 0x000000,
      BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
     "hash to sign:",
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},
     {{BAGL_LABELINE, 0x02, 23, 26, 82, 12, 0x80 | 10, 0, 0, 0xFFFFFF, 0x000000,
       BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
       vars.tmp.fullAddress,
     0,
     0,
     0,
     NULL,
     NULL,
     NULL},

};

unsigned int ui_verify_message_signature_nanos_button(unsigned int button_mask,
                                         unsigned int button_mask_counter);

unsigned int ui_verify_message_prepro(const bagl_element_t *element) {
    if (element->component.userid > 0) {
        unsigned int display = (ux_step == element->component.userid - 1);
        if (display) {
            switch (element->component.userid) {
            case 1:
                UX_CALLBACK_SET_INTERVAL(2000);
                break;
            case 2:
                UX_CALLBACK_SET_INTERVAL(MAX(
                    3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
                break;
            }
        }
        return display;
    }
    return 1;
}

void ui_idle(void) {
    ux_step_count = 0;
    UX_MENU_DISPLAY(0, menu_main, NULL);
}

unsigned int io_seproxyhal_touch_verify_cancel(const bagl_element_t *e) {
    // user denied the transaction, tell the USB side
    if (!btchip_bagl_user_action(0)) {
        // redraw ui
        ui_idle();
    }
    return 0; // DO NOT REDRAW THE BUTTON
}

unsigned int io_seproxyhal_touch_verify_ok(const bagl_element_t *e) {
    // user accepted the transaction, tell the USB side
    if (!btchip_bagl_user_action(1)) {
        // redraw ui
        ui_idle();
    }
    return 0; // DO NOT REDRAW THE BUTTON
}

unsigned int
io_seproxyhal_touch_message_signature_verify_cancel(const bagl_element_t *e) {
    // user denied the transaction, tell the USB side
    btchip_bagl_user_action_message_signing(0);
    // redraw ui
    ui_idle();
    return 0; // DO NOT REDRAW THE BUTTON
}

unsigned int
io_seproxyhal_touch_message_signature_verify_ok(const bagl_element_t *e) {
    // user accepted the transaction, tell the USB side
    btchip_bagl_user_action_message_signing(1);
    // redraw ui
    ui_idle();
    return 0; // DO NOT REDRAW THE BUTTON
}

unsigned int io_seproxyhal_touch_display_cancel(const bagl_element_t *e) {
    // user denied the transaction, tell the USB side
    btchip_bagl_user_action_display(0);
    // redraw ui
    ui_idle();
    return 0; // DO NOT REDRAW THE BUTTON
}

unsigned int io_seproxyhal_touch_display_ok(const bagl_element_t *e) {
    // user accepted the transaction, tell the USB side
    btchip_bagl_user_action_display(1);
    // redraw ui
    ui_idle();
    return 0; // DO NOT REDRAW THE BUTTON
}

unsigned int ui_verify_message_signature_nanos_button(unsigned int button_mask,
                                         unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_message_signature_verify_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        io_seproxyhal_touch_message_signature_verify_ok(NULL);
        break;
    }
    return 0;
}

unsigned int ui_display_address_nanos_button(unsigned int button_mask,
                                             unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_display_cancel(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        io_seproxyhal_touch_display_ok(NULL);
        break;
    }
    return 0;
}

// override point, but nothing more to do
void io_seproxyhal_display(const bagl_element_t *element) {
    if ((element->component.type & (~BAGL_TYPE_FLAGS_MASK)) != BAGL_NONE) {
        io_seproxyhal_display_default((bagl_element_t *)element);
    }
}

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_STATUS_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
            !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
              SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
            THROW(EXCEPTION_IO_RESET);
        }
    // no break is intentional
    default:
        UX_DEFAULT_EVENT();
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {
            // don't redisplay if UX not allowed (pin locked in the common bolos
            // ux ?)
            if (ux_step_count && UX_ALLOWED) {
                // prepare next screen
                ux_step = (ux_step + 1) % ux_step_count;
                // redisplay screen
                UX_REDISPLAY();
            }
        });
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

#define HASH_LENGTH 4
uint8_t prepare_message_signature() {
    cx_hash(&btchip_context_D.transactionHashAuthorization.header, CX_LAST, vars.tmp.fullAmount, 0, vars.tmp.fullAmount);
    snprintf(vars.tmp.fullAddress, sizeof(vars.tmp.fullAddress), "%.*H...%.*H", 8, vars.tmp.fullAmount, 8, vars.tmp.fullAmount + 32 - 8);
    return 1;
}

void btchip_bagl_confirm_message_signature() {
    if (!prepare_message_signature()) {
        return;
    }

    strcpy(vars.tmp.fullAddress, btchip_context_D.tmpmessaddr);
    ux_step = 0;
    ux_step_count = 2;
    UX_DISPLAY(ui_verify_message_signature_nanos, ui_verify_message_prepro);
}

unsigned int btchip_bagl_display_public_key() {
    // setup qrcode of the address in the apdu buffer
    strcat(G_io_apdu_buffer + 200, " ");

    // append and prepend a white space to the address
    G_io_apdu_buffer[199] = ' ';
    ux_step = 0;
    ux_step_count = 2;
    UX_DISPLAY(ui_display_address_nanos, ui_display_address_nanos_prepro);
    return 1;
}

void app_exit(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        UX_INIT();
        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

                btchip_context_init();

                // deactivate usb before activating
                USB_power_U2F(0, 0);

                os_memset((unsigned char *)&u2fService, 0, sizeof(u2fService));
                u2fService.inputBuffer = G_io_apdu_buffer;
                u2fService.outputBuffer = G_io_apdu_buffer;
                u2fService.messageBuffer = (uint8_t *)u2fMessageBuffer;
                u2fService.messageBufferSize = U2F_MAX_MESSAGE_SIZE;
                u2f_initialize_service((u2f_service_t *)&u2fService);

                //Force the user of U2F (Browser mode)
                if (N_btchip.fidoTransport == 0)
                {
                  unsigned int fidotransport = 1;
                  nvm_write(&N_btchip.fidoTransport, (void *)&fidotransport, sizeof(uint8_t));
                }

                USB_power_U2F(1, N_btchip.fidoTransport);

                ui_idle();

                app_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
                // reset IO and UX
                continue;
            }
            CATCH_ALL {
                break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }
    app_exit();

    return 0;
}
