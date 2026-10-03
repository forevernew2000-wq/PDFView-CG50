#include <gint/display.h>
#include <gint/keyboard.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define ROWS 8

typedef struct {
    int deg;
    int min;
} coord_t;

typedef struct {
    uint32_t radius;
    uint32_t scale;
    coord_t phi[3];
    coord_t lambda[3];
} input_t;

typedef struct {
    double delta;
    double m;
    double y;
    double x;
    double y_cm;
    double x_cm;
} result_t;

static const char POINT_LABELS[9] = {'A','D','G','B','E','H','C','F','I'};

static input_t in = {
    .radius = 6378137,
    .scale = 400000,
    .phi = {{10,0},{10,10},{10,20}},
    .lambda = {{50,30},{50,20},{50,10}},
};

static double rad(double deg)
{
    return deg * PI / 180.0;
}

static double coord_decimal(coord_t c)
{
    return (double)c.deg + (double)c.min / 60.0;
}

static void fmt_fixed(char *buf, size_t size, double value, int decimals)
{
    int negative = value < 0.0;
    if(negative) value = -value;

    long factor = 1;
    for(int i = 0; i < decimals; i++) factor *= 10;

    long scaled = (long)(value * (double)factor + 0.5);
    long whole = scaled / factor;
    long frac = scaled % factor;

    if(decimals == 0) {
        snprintf(buf, size, "%s%ld", negative ? "-" : "", whole);
    }
    else if(decimals == 1) {
        snprintf(buf, size, "%s%ld.%01ld", negative ? "-" : "", whole, frac);
    }
    else {
        snprintf(buf, size, "%s%ld.%02ld", negative ? "-" : "", whole, frac);
    }
}

static void draw_centered(int y, char const *text)
{
    int w = 0, h = 0;
    dsize(text, dfont_default(), &w, &h);
    (void)h;
    dtext((DWIDTH - w) / 2, y, C_BLACK, text);
}

static void message(char const *title, char const *line1, char const *line2)
{
    dclear(C_WHITE);
    draw_centered(35, title);
    if(line1) draw_centered(80, line1);
    if(line2) draw_centered(105, line2);
    draw_centered(172, "Presiona una tecla");
    dupdate();
    getkey();
}

static int key_digit(int key)
{
    switch(key) {
        case KEY_0: return 0;
        case KEY_1: return 1;
        case KEY_2: return 2;
        case KEY_3: return 3;
        case KEY_4: return 4;
        case KEY_5: return 5;
        case KEY_6: return 6;
        case KEY_7: return 7;
        case KEY_8: return 8;
        case KEY_9: return 9;
        default: return -1;
    }
}

static uint32_t edit_uint(char const *title, uint32_t current,
    uint32_t min_value, uint32_t max_value)
{
    char buf[16];
    snprintf(buf, sizeof buf, "%lu", (unsigned long)current);
    int len = strlen(buf);

    while(1) {
        dclear(C_WHITE);
        draw_centered(28, title);
        drect(45, 72, DWIDTH - 46, 116, C_BLACK);
        dtext(60, 84, C_WHITE, buf);
        dtext(12, 145, C_BLACK, "0-9: escribir   DEL: borrar");
        dtext(12, 168, C_BLACK, "EXE: aceptar    EXIT: cancelar");
        dupdate();

        int key = getkey().key;
        int digit = key_digit(key);

        if(digit >= 0 && len < (int)sizeof(buf) - 1) {
            if(len == 1 && buf[0] == '0') len = 0;
            buf[len++] = '0' + digit;
            buf[len] = 0;
        }
        else if(key == KEY_DEL && len > 0) {
            len--;
            buf[len] = 0;
        }
        else if(key == KEY_EXIT) {
            return current;
        }
        else if(key == KEY_EXE && len > 0) {
            unsigned long value = strtoul(buf, NULL, 10);
            if(value >= min_value && value <= max_value) {
                return (uint32_t)value;
            }

            message("Valor fuera de rango", "Revisa el numero", NULL);
        }
    }
}

static void edit_coord(char const *name, coord_t *c, int max_deg)
{
    char title[48];

    snprintf(title, sizeof title, "%s - grados", name);
    int deg = (int)edit_uint(title, c->deg, 0, max_deg);

    snprintf(title, sizeof title, "%s - minutos", name);
    int min = (int)edit_uint(title, c->min, 0, 59);

    c->deg = deg;
    c->min = min;
}

static void show_formula(int mode)
{
    dclear(C_WHITE);
    dtext(8, 10, C_BLACK, mode == 0 ? "EQUIDISTANTE MERIDIANA" : "EQUIDISTANTE TRANSVERSAL");
    dtext(8, 42, C_BLACK, "delta = 90 - phi");

    if(mode == 0) {
        dtext(8, 70, C_BLACK, "m = R * delta (radianes)");
    }
    else {
        dtext(8, 70, C_BLACK, "m = R * seno(delta)");
    }

    dtext(8, 98, C_BLACK, "Y = m * seno(lambda)");
    dtext(8, 126, C_BLACK, "X = m * cos(lambda)");
    dtext(8, 154, C_BLACK, "cm = metros / escala * 100");
    dtext(8, 190, C_BLACK, "Basado en tu Excel");
    dupdate();
    getkey();
}

static void draw_main_menu(void)
{
    dclear(C_WHITE);
    draw_centered(25, "PROYCALC - fx-CG50");
    draw_centered(58, "Proyecciones planas");

    drect(28, 92, DWIDTH - 29, 122, C_BLACK);
    draw_centered(99, "F1  Equidistante Meridiana");

    drect(28, 136, DWIDTH - 29, 166, C_BLACK);
    draw_centered(143, "F2  Equidistante Transversal");

    draw_centered(190, "EXIT para salir");
    dupdate();
}

static void format_coord(char *buf, size_t n, coord_t c)
{
    snprintf(buf, n, "%d deg %02d min", c.deg, c.min);
}

static void draw_input_screen(int mode, int selected)
{
    char buf[64];

    dclear(C_WHITE);
    dtext(7, 5, C_BLACK, mode == 0 ? "MERIDIANA - DATOS" : "TRANSVERSAL - DATOS");

    for(int row = 0; row < ROWS; row++) {
        int y = 29 + row * 20;
        int fg = C_BLACK;
        int bg = C_WHITE;

        if(row == selected) {
            drect(4, y - 2, DWIDTH - 5, y + 16, C_BLACK);
            fg = C_WHITE;
            bg = C_BLACK;
        }

        (void)bg;

        if(row == 0) {
            snprintf(buf, sizeof buf, "Radio WGS84: %lu", (unsigned long)in.radius);
        }
        else if(row == 1) {
            snprintf(buf, sizeof buf, "Escala 1:%lu", (unsigned long)in.scale);
        }
        else if(row >= 2 && row <= 4) {
            char cbuf[32];
            format_coord(cbuf, sizeof cbuf, in.phi[row - 2]);
            snprintf(buf, sizeof buf, "phi%d: %s", row - 1, cbuf);
        }
        else {
            char cbuf[32];
            format_coord(cbuf, sizeof cbuf, in.lambda[row - 5]);
            snprintf(buf, sizeof buf, "lambda%d: %s", row - 4, cbuf);
        }

        dtext(10, y, fg, buf);
    }

    dtext(6, 194, C_BLACK, "EXE editar  F1 calcular  F6 formulas");
    dupdate();
}

static void compute_results(int mode, result_t out[9])
{
    for(int p = 0; p < 9; p++) {
        int phi_i = p % 3;
        int lambda_i = p / 3;

        double phi = coord_decimal(in.phi[phi_i]);
        double lambda = coord_decimal(in.lambda[lambda_i]);
        double delta = 90.0 - phi;

        double m;
        if(mode == 0) {
            m = (double)in.radius * rad(delta);
        }
        else {
            m = (double)in.radius * sin(rad(delta));
        }

        double y = m * sin(rad(lambda));
        double x = m * cos(rad(lambda));

        out[p].delta = delta;
        out[p].m = m;
        out[p].y = y;
        out[p].x = x;
        out[p].y_cm = y / (double)in.scale * 100.0;
        out[p].x_cm = x / (double)in.scale * 100.0;
    }
}

static void draw_summary(result_t r[9])
{
    double ymax = r[0].y, ymin = r[0].y;
    double xmax = r[0].x, xmin = r[0].x;

    for(int i = 1; i < 9; i++) {
        if(r[i].y > ymax) ymax = r[i].y;
        if(r[i].y < ymin) ymin = r[i].y;
        if(r[i].x > xmax) xmax = r[i].x;
        if(r[i].x < xmin) xmin = r[i].x;
    }

    double dy = ymax - ymin;
    double dx = xmax - xmin;
    double dy_cm = dy / (double)in.scale * 100.0;
    double dx_cm = dx / (double)in.scale * 100.0;

    char a[32], b[32];

    dclear(C_WHITE);
    dtext(8, 7, C_BLACK, "RESUMEN / DIMENSIONES");

    fmt_fixed(a, sizeof a, ymax, 2);
    fmt_fixed(b, sizeof b, ymin, 2);
    dprint(8, 36, C_BLACK, "Y max: %s", a);
    dprint(8, 56, C_BLACK, "Y min: %s", b);
    fmt_fixed(a, sizeof a, dy, 2);
    dprint(8, 76, C_BLACK, "Delta Y: %s m", a);

    fmt_fixed(a, sizeof a, xmax, 2);
    fmt_fixed(b, sizeof b, xmin, 2);
    dprint(8, 105, C_BLACK, "X max: %s", a);
    dprint(8, 125, C_BLACK, "X min: %s", b);
    fmt_fixed(a, sizeof a, dx, 2);
    dprint(8, 145, C_BLACK, "Delta X: %s m", a);

    fmt_fixed(a, sizeof a, dy_cm, 2);
    fmt_fixed(b, sizeof b, dx_cm, 2);
    dprint(8, 176, C_BLACK, "Plano: dY=%s cm  dX=%s cm", a, b);

    dtext(8, 202, C_BLACK, "Cualquier tecla para volver");
    dupdate();
    getkey();
}

static void draw_point_detail(int mode, int p, result_t *r)
{
    int phi_i = p % 3;
    int lambda_i = p / 3;

    char buf[40];

    dclear(C_WHITE);
    dprint(8, 7, C_BLACK, "PUNTO %c - %s", POINT_LABELS[p],
        mode == 0 ? "MERID" : "TRANSV");

    dprint(8, 34, C_BLACK, "phi: %d deg %02d min",
        in.phi[phi_i].deg, in.phi[phi_i].min);
    dprint(8, 54, C_BLACK, "lambda: %d deg %02d min",
        in.lambda[lambda_i].deg, in.lambda[lambda_i].min);

    fmt_fixed(buf, sizeof buf, r->delta, 2);
    dprint(8, 78, C_BLACK, "delta: %s deg", buf);

    fmt_fixed(buf, sizeof buf, r->m, 2);
    dprint(8, 100, C_BLACK, "m: %s", buf);

    fmt_fixed(buf, sizeof buf, r->y, 2);
    dprint(8, 122, C_BLACK, "Y: %s m", buf);

    fmt_fixed(buf, sizeof buf, r->x, 2);
    dprint(8, 144, C_BLACK, "X: %s m", buf);

    fmt_fixed(buf, sizeof buf, r->y_cm, 2);
    dprint(8, 166, C_BLACK, "Y plano: %s cm", buf);

    fmt_fixed(buf, sizeof buf, r->x_cm, 2);
    dprint(8, 188, C_BLACK, "X plano: %s cm", buf);

    dupdate();
    getkey();
}

static void draw_results(int mode, result_t r[9], int selected, int unit_mode)
{
    dclear(C_WHITE);
    dprint(6, 5, C_BLACK, "%s  1:%lu",
        mode == 0 ? "MERIDIANA" : "TRANSVERSAL",
        (unsigned long)in.scale);

    if(unit_mode == 0) {
        dtext(8, 26, C_BLACK, "P       Y cm             X cm");
    }
    else {
        dtext(8, 26, C_BLACK, "P       Y metros         X metros");
    }

    for(int i = 0; i < 9; i++) {
        int y = 45 + i * 16;
        int fg = C_BLACK;

        if(i == selected) {
            drect(4, y - 2, DWIDTH - 5, y + 13, C_BLACK);
            fg = C_WHITE;
        }

        char ys[28], xs[28];

        if(unit_mode == 0) {
            fmt_fixed(ys, sizeof ys, r[i].y_cm, 2);
            fmt_fixed(xs, sizeof xs, r[i].x_cm, 2);
        }
        else {
            fmt_fixed(ys, sizeof ys, r[i].y, 0);
            fmt_fixed(xs, sizeof xs, r[i].x, 0);
        }

        dprint(10, y, fg, "%c", POINT_LABELS[i]);
        dtext(58, y, fg, ys);
        dtext(205, y, fg, xs);
    }

    dtext(5, 195, C_BLACK, "F1 unidad F2 resumen EXE detalle EXIT");
    dupdate();
}

static void results_screen(int mode)
{
    result_t r[9];
    compute_results(mode, r);

    int selected = 0;
    int unit_mode = 0;

    while(1) {
        draw_results(mode, r, selected, unit_mode);
        int key = getkey().key;

        if(key == KEY_EXIT) return;
        if(key == KEY_UP && selected > 0) selected--;
        else if(key == KEY_DOWN && selected < 8) selected++;
        else if(key == KEY_F1) unit_mode = !unit_mode;
        else if(key == KEY_F2) draw_summary(r);
        else if(key == KEY_EXE) draw_point_detail(mode, selected, &r[selected]);
    }
}

static void input_screen(int mode)
{
    int selected = 0;

    while(1) {
        draw_input_screen(mode, selected);
        int key = getkey().key;

        if(key == KEY_EXIT) return;

        if(key == KEY_UP && selected > 0) {
            selected--;
        }
        else if(key == KEY_DOWN && selected < ROWS - 1) {
            selected++;
        }
        else if(key == KEY_F6) {
            show_formula(mode);
        }
        else if(key == KEY_F1) {
            if(in.radius == 0 || in.scale == 0) {
                message("Datos invalidos", "Radio y escala > 0", NULL);
            }
            else {
                results_screen(mode);
            }
        }
        else if(key == KEY_EXE) {
            if(selected == 0) {
                in.radius = edit_uint("Radio WGS84", in.radius, 1, 99999999);
            }
            else if(selected == 1) {
                in.scale = edit_uint("Denominador escala", in.scale, 1, 999999999);
            }
            else if(selected >= 2 && selected <= 4) {
                char name[20];
                snprintf(name, sizeof name, "phi%d", selected - 1);
                edit_coord(name, &in.phi[selected - 2], 90);
            }
            else {
                char name[20];
                snprintf(name, sizeof name, "lambda%d", selected - 4);
                edit_coord(name, &in.lambda[selected - 5], 359);
            }
        }
    }
}

int main(void)
{
    while(1) {
        draw_main_menu();
        int key = getkey().key;

        if(key == KEY_EXIT) break;
        if(key == KEY_F1) input_screen(0);
        else if(key == KEY_F2) input_screen(1);
    }

    return 1;
}
