#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/gint.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DOCUMENT_PATH "/PDFVIEW.CGV"
#define HEADER_SIZE 16
#define INDEX_RECORD_SIZE 20

#define FIT_WIDTH 384
#define FIT_HEIGHT 216
#define FIT_STRIDE ((FIT_WIDTH + 7) / 8)
#define FIT_BYTES (FIT_STRIDE * FIT_HEIGHT)

#define MAX_ZOOM_BYTES (128 * 1024)
#define PAN_STEP 48

static const uint8_t MAGIC[8] = {'C','G','P','D','F','1',0,0};

typedef struct {
    uint16_t page_count;
    uint32_t index_offset;
} document_t;

typedef struct {
    document_t *doc;
    uint16_t page;
    uint16_t zoom_w;
    uint16_t zoom_h;
    uint32_t zoom_size;
    uint8_t *fit_data;
    uint8_t *zoom_data;
} page_request_t;

static document_t doc;
static uint8_t fit_buffer[FIT_BYTES];
static uint8_t zoom_buffer[MAX_ZOOM_BYTES];

static uint16_t read_be16(uint8_t const *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t read_be32(uint8_t const *p)
{
    return ((uint32_t)p[0] << 24)
         | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)
         | p[3];
}

static int bit_get(uint8_t const *data, int stride, int x, int y)
{
    uint8_t byte = data[y * stride + (x >> 3)];
    return (byte & (0x80u >> (x & 7))) != 0;
}

/* File operations run in the calculator OS world. */
static int os_load_header(document_t *out)
{
    uint8_t h[HEADER_SIZE];
    FILE *f = fopen(DOCUMENT_PATH, "rb");
    if(!f) return -1;

    size_t n = fread(h, 1, sizeof h, f);
    fclose(f);

    if(n != sizeof h) return -2;
    if(memcmp(h, MAGIC, sizeof MAGIC) != 0) return -3;

    out->page_count = read_be16(h + 8);
    out->index_offset = read_be32(h + 12);

    if(out->page_count == 0) return -4;
    if(out->index_offset < HEADER_SIZE) return -5;
    return 0;
}

static int os_load_page(page_request_t *r)
{
    uint8_t rec[INDEX_RECORD_SIZE];

    FILE *f = fopen(DOCUMENT_PATH, "rb");
    if(!f) return -1;

    if(r->page >= r->doc->page_count) {
        fclose(f);
        return -2;
    }

    long rec_pos = (long)r->doc->index_offset
                 + (long)r->page * INDEX_RECORD_SIZE;

    if(fseek(f, rec_pos, SEEK_SET) != 0 ||
       fread(rec, 1, sizeof rec, f) != sizeof rec) {
        fclose(f);
        return -3;
    }

    r->zoom_w = read_be16(rec + 0);
    r->zoom_h = read_be16(rec + 2);

    uint32_t zoom_off = read_be32(rec + 4);
    r->zoom_size = read_be32(rec + 8);
    uint32_t fit_off = read_be32(rec + 12);
    uint32_t fit_size = read_be32(rec + 16);

    uint32_t expected_zoom =
        (uint32_t)((r->zoom_w + 7) / 8) * (uint32_t)r->zoom_h;

    if(r->zoom_w == 0 || r->zoom_h == 0) {
        fclose(f);
        return -4;
    }

    if(r->zoom_size != expected_zoom || r->zoom_size > MAX_ZOOM_BYTES) {
        fclose(f);
        return -5;
    }

    if(fit_size != FIT_BYTES) {
        fclose(f);
        return -6;
    }

    if(fseek(f, (long)fit_off, SEEK_SET) != 0 ||
       fread(r->fit_data, 1, FIT_BYTES, f) != FIT_BYTES) {
        fclose(f);
        return -7;
    }

    if(fseek(f, (long)zoom_off, SEEK_SET) != 0 ||
       fread(r->zoom_data, 1, r->zoom_size, f) != r->zoom_size) {
        fclose(f);
        return -8;
    }

    fclose(f);
    return 0;
}

static void draw_text_centered(int y, char const *text)
{
    int w = 0, h = 0;
    dsize(text, dfont_default(), &w, &h);
    (void)h;
    dtext((DWIDTH - w) / 2, y, C_BLACK, text);
}

static void show_message(char const *title, char const *line1,
    char const *line2)
{
    dclear(C_WHITE);
    draw_text_centered(35, title);
    if(line1) draw_text_centered(80, line1);
    if(line2) draw_text_centered(105, line2);
    draw_text_centered(165, "Press any key");
    dupdate();
    getkey();
}

static void show_help(void)
{
    dclear(C_WHITE);
    dtext(8, 8, C_BLACK, "PDFView CG50");
    dtext(8, 34, C_BLACK, "F1/F2: previous/next page");
    dtext(8, 58, C_BLACK, "+ : zoom in (1x/2x/3x)");
    dtext(8, 82, C_BLACK, "- : zoom out / fit");
    dtext(8, 106, C_BLACK, "Arrows: pan while zoomed");
    dtext(8, 130, C_BLACK, "LEFT/RIGHT: page in fit mode");
    dtext(8, 154, C_BLACK, "F6: help");
    dtext(8, 178, C_BLACK, "EXIT: quit");
    dtext(8, 202, C_BLACK, "File: /PDFVIEW.CGV");
    dupdate();
    getkey();
}

static void show_loading(uint16_t page, uint16_t pages)
{
    char buf[48];
    snprintf(buf, sizeof buf, "Loading page %u / %u...",
        (unsigned)(page + 1), (unsigned)pages);

    dclear(C_WHITE);
    draw_text_centered(90, buf);
    dupdate();
}

static void draw_horizontal_runs(uint8_t const *data, int stride,
    int src_x, int src_y, int width, int height, int dst_x, int dst_y)
{
    for(int y = 0; y < height; y++) {
        int run = -1;

        for(int x = 0; x <= width; x++) {
            int black = 0;

            if(x < width) {
                black = bit_get(data, stride, src_x + x, src_y + y);
            }

            if(black && run < 0) run = x;

            if(!black && run >= 0) {
                dline(dst_x + run, dst_y + y,
                      dst_x + x - 1, dst_y + y, C_BLACK);
                run = -1;
            }
        }
    }
}

static void draw_fit(void)
{
    int dst_x = (DWIDTH - FIT_WIDTH) / 2;
    int dst_y = (DHEIGHT - FIT_HEIGHT) / 2;

    dclear(C_WHITE);
    draw_horizontal_runs(fit_buffer, FIT_STRIDE, 0, 0,
        FIT_WIDTH, FIT_HEIGHT, dst_x, dst_y);
    dupdate();
}

static void clamp_pan(int *pan_x, int *pan_y,
    int zoom_w, int zoom_h, int scale)
{
    int view_w = DWIDTH / scale;
    int view_h = DHEIGHT / scale;
    int max_x = zoom_w > view_w ? zoom_w - view_w : 0;
    int max_y = zoom_h > view_h ? zoom_h - view_h : 0;

    if(*pan_x < 0) *pan_x = 0;
    if(*pan_y < 0) *pan_y = 0;
    if(*pan_x > max_x) *pan_x = max_x;
    if(*pan_y > max_y) *pan_y = max_y;
}

static void reset_pan(int *pan_x, int *pan_y,
    int zoom_w, int zoom_h, int scale)
{
    int view_w = DWIDTH / scale;
    *pan_x = zoom_w > view_w ? (zoom_w - view_w) / 2 : 0;
    *pan_y = 0;
    clamp_pan(pan_x, pan_y, zoom_w, zoom_h, scale);
}

static void draw_zoom(int pan_x, int pan_y,
    int zoom_w, int zoom_h, int scale)
{
    int stride = (zoom_w + 7) / 8;
    int view_w = DWIDTH / scale;
    int view_h = DHEIGHT / scale;
    int visible_w = zoom_w - pan_x;
    int visible_h = zoom_h - pan_y;

    if(visible_w > view_w) visible_w = view_w;
    if(visible_h > view_h) visible_h = view_h;

    int dst_w = visible_w * scale;
    int dst_h = visible_h * scale;
    int dst_x = (DWIDTH - dst_w) / 2;
    int dst_y = (DHEIGHT - dst_h) / 2;

    dclear(C_WHITE);

    for(int y = 0; y < visible_h; y++) {
        int run = -1;

        for(int x = 0; x <= visible_w; x++) {
            int black = 0;

            if(x < visible_w) {
                black = bit_get(zoom_buffer, stride,
                    pan_x + x, pan_y + y);
            }

            if(black && run < 0) run = x;

            if(!black && run >= 0) {
                drect(
                    dst_x + run * scale,
                    dst_y + y * scale,
                    dst_x + x * scale - 1,
                    dst_y + (y + 1) * scale - 1,
                    C_BLACK);
                run = -1;
            }
        }
    }

    dupdate();
}

static int load_page(uint16_t page, page_request_t *req)
{
    req->doc = &doc;
    req->page = page;
    req->fit_data = fit_buffer;
    req->zoom_data = zoom_buffer;

    return gint_world_switch(GINT_CALL(os_load_page, (void *)req));
}

int main(void)
{
    int rc = gint_world_switch(GINT_CALL(os_load_header, (void *)&doc));

    if(rc != 0) {
        show_message("PDFView CG50",
            "Cannot open /PDFVIEW.CGV",
            "Copy PDFVIEW.CGV to storage root.");
        return 1;
    }

    uint16_t page = 0;
    page_request_t req;
    memset(&req, 0, sizeof req);

    show_loading(page, doc.page_count);
    rc = load_page(page, &req);

    if(rc != 0) {
        show_message("PDFView CG50",
            "Invalid or unsupported CGV.",
            "Convert the PDF again.");
        return 1;
    }

    int zoom_level = 0;
    int pan_x = 0;
    int pan_y = 0;

    draw_fit();

    while(1) {
        int key = getkey().key;
        int need_redraw = 0;
        int page_changed = 0;

        if(key == KEY_EXIT) break;

        if(key == KEY_F6) {
            show_help();
            need_redraw = 1;
        }
        else if(key == KEY_ADD) {
            if(zoom_level < 3) {
                zoom_level++;

                if(zoom_level == 1) {
                    reset_pan(&pan_x, &pan_y,
                        req.zoom_w, req.zoom_h, zoom_level);
                }
                else {
                    clamp_pan(&pan_x, &pan_y,
                        req.zoom_w, req.zoom_h, zoom_level);
                }

                need_redraw = 1;
            }
        }
        else if(key == KEY_SUB) {
            if(zoom_level > 0) {
                zoom_level--;

                if(zoom_level > 0) {
                    clamp_pan(&pan_x, &pan_y,
                        req.zoom_w, req.zoom_h, zoom_level);
                }

                need_redraw = 1;
            }
        }
        else if(key == KEY_F1 ||
            (zoom_level == 0 && key == KEY_LEFT)) {
            if(page > 0) {
                page--;
                page_changed = 1;
            }
        }
        else if(key == KEY_F2 ||
            (zoom_level == 0 && key == KEY_RIGHT)) {
            if(page + 1 < doc.page_count) {
                page++;
                page_changed = 1;
            }
        }
        else if(zoom_level > 0) {
            int step = PAN_STEP / zoom_level;
            if(step < 1) step = 1;

            if(key == KEY_LEFT)  { pan_x -= step; need_redraw = 1; }
            if(key == KEY_RIGHT) { pan_x += step; need_redraw = 1; }
            if(key == KEY_UP)    { pan_y -= step; need_redraw = 1; }
            if(key == KEY_DOWN)  { pan_y += step; need_redraw = 1; }

            clamp_pan(&pan_x, &pan_y,
                req.zoom_w, req.zoom_h, zoom_level);
        }

        if(page_changed) {
            show_loading(page, doc.page_count);
            rc = load_page(page, &req);

            if(rc != 0) {
                show_message("PDFView CG50",
                    "Could not load this page.",
                    "The CGV file may be damaged.");
                break;
            }

            if(zoom_level > 0) {
                reset_pan(&pan_x, &pan_y,
                    req.zoom_w, req.zoom_h, zoom_level);
            }

            need_redraw = 1;
        }

        if(need_redraw) {
            if(zoom_level > 0) {
                draw_zoom(pan_x, pan_y,
                    req.zoom_w, req.zoom_h, zoom_level);
            }
            else {
                draw_fit();
            }
        }
    }

    return 1;
}
