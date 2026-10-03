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

static const char POINT_LABELS[9] = {'A','B','C','D','E','F','G','H','I'};

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

static void draw_box(int x1, int y1, int x2, int y2, int filled)
{
    if(filled) {
        drect(x1, y1, x2, y2, C_BLACK);
        return;
    }

    dline(x1, y1, x2, y1, C_BLACK);
    dline(x1, y2, x2, y2, C_BLACK);
    dline(x1, y1, x1, y2, C_BLACK);
    dline(x2, y1, x2, y2, C_BLACK);
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
        draw_centered(12, title);
        draw_centered(39, "Escribe el nuevo valor");

        draw_box(54, 72, DWIDTH - 55, 119, 1);
        dtext(70, 87, C_WHITE, buf);

        dtext(18, 149, C_BLACK, "0-9 escribir     DEL borrar");
        dtext(18, 174, C_BLACK, "EXE guardar      EXIT cancelar");
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

            message("Valor fuera de rango", "Revisa el numero ingresado", NULL);
        }
    }
}

static void edit_coord(char const *name, coord_t *c, int max_deg)
{
    int original_deg = c->deg;
    int original_min = c->min;
    int deg = c->deg;
    int min = c->min;
    int field = 0;
    int fresh = 1;

    while(1) {
        char deg_text[16], min_text[16], complete[64];

        snprintf(deg_text, sizeof deg_text, "%d", deg);
        snprintf(min_text, sizeof min_text, "%02d", min);
        snprintf(complete, sizeof complete, "%s = %d deg %02d'", name, deg, min);

        dclear(C_WHITE);
        dprint(8, 7, C_BLACK, "EDITAR %s", name);
        dtext(8, 30, C_BLACK, "Selecciona grados o minutos");

        dtext(77, 60, C_BLACK, "GRADOS");
        dtext(239, 60, C_BLACK, "MINUTOS");

        draw_box(47, 82, 164, 127, field == 0);
        draw_box(211, 82, 328, 127, field == 1);

        dtext(82, 96, field == 0 ? C_WHITE : C_BLACK, deg_text);
        dtext(250, 96, field == 1 ? C_WHITE : C_BLACK, min_text);

        draw_centered(145, complete);
        dtext(13, 173, C_BLACK, "<- -> cambiar   0-9 escribir");
        dtext(13, 195, C_BLACK, "DEL borrar  EXE guardar  EXIT cancelar");
        dupdate();

        int key = getkey().key;
        int digit = key_digit(key);

        if(key == KEY_LEFT || key == KEY_RIGHT) {
            field = 1 - field;
            fresh = 1;
        }
        else if(key == KEY_UP || key == KEY_DOWN) {
            field = 1 - field;
            fresh = 1;
        }
        else if(digit >= 0) {
            int *value = field == 0 ? &deg : &min;
            int limit = field == 0 ? max_deg : 59;
            int candidate = fresh ? digit : (*value * 10 + digit);

            if(candidate <= limit) {
                *value = candidate;
                fresh = 0;
            }
        }
        else if(key == KEY_DEL) {
            int *value = field == 0 ? &deg : &min;
            *value /= 10;
            fresh = 0;
        }
        else if(key == KEY_EXE) {
            c->deg = deg;
            c->min = min;
            return;
        }
        else if(key == KEY_EXIT) {
            c->deg = original_deg;
            c->min = original_min;
            return;
        }
    }
}

static void show_formula(int mode)
{
    dclear(C_WHITE);
    dtext(8, 8, C_BLACK,
        mode == 0 ? "EQUIDISTANTE MERIDIANA" : "EQUIDISTANTE TRANSVERSAL");

    dtext(8, 39, C_BLACK, "delta = 90 - phi");

    if(mode == 0) {
        dtext(8, 67, C_BLACK, "m = R * delta (en radianes)");
    }
    else {
        dtext(8, 67, C_BLACK, "m = R * seno(delta)");
    }

    dtext(8, 95, C_BLACK, "Y = m * seno(lambda)");
    dtext(8, 123, C_BLACK, "X = m * cos(lambda)");
    dtext(8, 151, C_BLACK, "Plano cm = metros / escala * 100");

    dline(8, 178, DWIDTH - 9, 178, C_BLACK);
    dtext(8, 190, C_BLACK, "Basado en tu Excel | tecla para volver");
    dupdate();
    getkey();
}

static void show_grid(void)
{
    dclear(C_WHITE);
    draw_centered(8, "MALLA DE PUNTOS");

    dtext(80, 40, C_BLACK, "lambda1");
    dtext(177, 40, C_BLACK, "lambda2");
    dtext(274, 40, C_BLACK, "lambda3");

    dtext(12, 78, C_BLACK, "phi1");
    dtext(12, 119, C_BLACK, "phi2");
    dtext(12, 160, C_BLACK, "phi3");

    int xs[3] = {105, 203, 300};
    int ys[3] = {78, 119, 160};
    char label = 'A';

    for(int r = 0; r < 3; r++) {
        for(int c = 0; c < 3; c++) {
            drect(xs[c] - 18, ys[r] - 6, xs[c] + 18, ys[r] + 20, C_BLACK);
            dprint(xs[c] - 4, ys[r], C_WHITE, "%c", label++);
        }
    }

    dtext(8, 196, C_BLACK, "A-I = combinaciones phi x lambda");
    dupdate();
    getkey();
}

static void draw_main_menu(int selected)
{
    dclear(C_WHITE);
    draw_centered(6, "PROYCALC v2 - fx-CG50");
    draw_centered(28, "Proyecciones planas");

    int y1 = 56;
    int y2 = 117;

    draw_box(18, y1, DWIDTH - 19, y1 + 51, selected == 0);
    dtext(32, y1 + 8, selected == 0 ? C_WHITE : C_BLACK,
        "1. EQUIDISTANTE MERIDIANA");
    dtext(32, y1 + 29, selected == 0 ? C_WHITE : C_BLACK,
        "m = R * delta");

    draw_box(18, y2, DWIDTH - 19, y2 + 51, selected == 1);
    dtext(32, y2 + 8, selected == 1 ? C_WHITE : C_BLACK,
        "2. EQUIDISTANTE TRANSVERSAL");
    dtext(32, y2 + 29, selected == 1 ? C_WHITE : C_BLACK,
        "m = R * seno(delta)");

    dtext(16, 187, C_BLACK, "ARRIBA/ABAJO elegir   EXE abrir");
    dtext(16, 203, C_BLACK, "F1/F2 acceso rapido    EXIT salir");
    dupdate();
}

static void format_coord(char *buf, size_t n, coord_t c)
{
    snprintf(buf, n, "%d deg %02d'", c.deg, c.min);
}

static void draw_input_screen(int mode, int selected)
{
    char label[32];
    char value[48];

    dclear(C_WHITE);
    dprint(7, 4, C_BLACK, "%s | DATOS",
        mode == 0 ? "MERIDIANA" : "TRANSVERSAL");
    dtext(7, 20, C_BLACK, "Selecciona una fila y presiona EXE");

    for(int row = 0; row < ROWS; row++) {
        int y = 40 + row * 18;
        int fg = C_BLACK;

        if(row == selected) {
            drect(4, y - 2, DWIDTH - 5, y + 14, C_BLACK);
            fg = C_WHITE;
        }

        if(row == 0) {
            snprintf(label, sizeof label, "Radio WGS84");
            snprintf(value, sizeof value, "%lu", (unsigned long)in.radius);
        }
        else if(row == 1) {
            snprintf(label, sizeof label, "Escala");
            snprintf(value, sizeof value, "1:%lu", (unsigned long)in.scale);
        }
        else if(row >= 2 && row <= 4) {
            snprintf(label, sizeof label, "phi%d", row - 1);
            format_coord(value, sizeof value, in.phi[row - 2]);
        }
        else {
            snprintf(label, sizeof label, "lambda%d", row - 4);
            format_coord(value, sizeof value, in.lambda[row - 5]);
        }

        dtext(12, y, fg, label);
        dtext(145, y, fg, value);
    }

    dline(4, 186, DWIDTH - 5, 186, C_BLACK);
    dtext(6, 190, C_BLACK, "F1 CALCULAR   F3 MALLA   F6 FORMULAS");
    dtext(6, 204, C_BLACK, "EXE editar               EXIT volver");
    dupdate();
}

static void compute_results(int mode, result_t out[9])
{
    for(int p = 0; p < 9; p++) {
        int phi_i = p / 3;
        int lambda_i = p % 3;

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
    draw_centered(7, "RESUMEN / DIMENSIONES");
    dline(8, 29, DWIDTH - 9, 29, C_BLACK);

    dtext(17, 42, C_BLACK, "EJE Y");
    fmt_fixed(a, sizeof a, ymax, 2);
    dprint(17, 65, C_BLACK, "Max: %s m", a);
    fmt_fixed(a, sizeof a, ymin, 2);
    dprint(17, 85, C_BLACK, "Min: %s m", a);
    fmt_fixed(a, sizeof a, dy, 2);
    dprint(17, 105, C_BLACK, "dY : %s m", a);
    fmt_fixed(a, sizeof a, dy_cm, 2);
    dprint(17, 125, C_BLACK, "Plano: %s cm", a);

    dline(DWIDTH / 2, 38, DWIDTH / 2, 151, C_BLACK);

    dtext(210, 42, C_BLACK, "EJE X");
    fmt_fixed(b, sizeof b, xmax, 2);
    dprint(210, 65, C_BLACK, "Max: %s m", b);
    fmt_fixed(b, sizeof b, xmin, 2);
    dprint(210, 85, C_BLACK, "Min: %s m", b);
    fmt_fixed(b, sizeof b, dx, 2);
    dprint(210, 105, C_BLACK, "dX : %s m", b);
    fmt_fixed(b, sizeof b, dx_cm, 2);
    dprint(210, 125, C_BLACK, "Plano: %s cm", b);

    dline(8, 163, DWIDTH - 9, 163, C_BLACK);
    dprint(17, 178, C_BLACK, "Escala 1:%lu", (unsigned long)in.scale);
    dtext(17, 198, C_BLACK, "Cualquier tecla para volver");
    dupdate();
    getkey();
}

static void draw_point_detail(int mode, int p, result_t *r)
{
    int phi_i = p / 3;
    int lambda_i = p % 3;
    char buf[40];

    dclear(C_WHITE);
    dprint(8, 6, C_BLACK, "PUNTO %c | %s", POINT_LABELS[p],
        mode == 0 ? "MERIDIANA" : "TRANSVERSAL");
    dline(8, 27, DWIDTH - 9, 27, C_BLACK);

    dprint(8, 39, C_BLACK, "phi%d    = %d deg %02d'",
        phi_i + 1, in.phi[phi_i].deg, in.phi[phi_i].min);
    dprint(8, 59, C_BLACK, "lambda%d = %d deg %02d'",
        lambda_i + 1, in.lambda[lambda_i].deg, in.lambda[lambda_i].min);

    fmt_fixed(buf, sizeof buf, r->delta, 2);
    dprint(8, 84, C_BLACK, "delta   = %s deg", buf);

    fmt_fixed(buf, sizeof buf, r->m, 2);
    dprint(8, 105, C_BLACK, "m       = %s", buf);

    fmt_fixed(buf, sizeof buf, r->y, 2);
    dprint(8, 128, C_BLACK, "Y real  = %s m", buf);

    fmt_fixed(buf, sizeof buf, r->x, 2);
    dprint(8, 149, C_BLACK, "X real  = %s m", buf);

    fmt_fixed(buf, sizeof buf, r->y_cm, 2);
    dprint(8, 172, C_BLACK, "Y plano = %s cm", buf);

    fmt_fixed(buf, sizeof buf, r->x_cm, 2);
    dprint(8, 193, C_BLACK, "X plano = %s cm", buf);

    dupdate();
    getkey();
}

static void draw_results(int mode, result_t r[9], int selected, int unit_mode)
{
    dclear(C_WHITE);

    dprint(6, 4, C_BLACK, "%s | 1:%lu",
        mode == 0 ? "MERIDIANA" : "TRANSVERSAL",
        (unsigned long)in.scale);

    dline(5, 23, DWIDTH - 6, 23, C_BLACK);

    if(unit_mode == 0) {
        dtext(10, 28, C_BLACK, "P");
        dtext(65, 28, C_BLACK, "Y PLANO (cm)");
        dtext(224, 28, C_BLACK, "X PLANO (cm)");
    }
    else {
        dtext(10, 28, C_BLACK, "P");
        dtext(65, 28, C_BLACK, "Y REAL (m)");
        dtext(224, 28, C_BLACK, "X REAL (m)");
    }

    for(int i = 0; i < 9; i++) {
        int y = 47 + i * 15;
        int fg = C_BLACK;

        if(i == selected) {
            drect(4, y - 2, DWIDTH - 5, y + 12, C_BLACK);
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

        dprint(11, y, fg, "%c", POINT_LABELS[i]);
        dtext(65, y, fg, ys);
        dtext(224, y, fg, xs);
    }

    dline(4, 184, DWIDTH - 5, 184, C_BLACK);
    dtext(5, 188, C_BLACK, "F1 UNIDAD  F2 RESUMEN  F3 MALLA");
    dtext(5, 203, C_BLACK, "EXE DETALLE              EXIT EDITAR");
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
        else if(key == KEY_F3) show_grid();
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
        else if(key == KEY_F3) {
            show_grid();
        }
        else if(key == KEY_F1) {
            if(in.radius == 0 || in.scale == 0) {
                message("Datos invalidos", "Radio y escala deben ser > 0", NULL);
            }
            else {
                results_screen(mode);
            }
        }
        else if(key == KEY_EXE) {
            if(selected == 0) {
                in.radius = edit_uint("RADIO WGS84", in.radius, 1, 99999999);
            }
            else if(selected == 1) {
                in.scale = edit_uint("DENOMINADOR DE ESCALA", in.scale, 1, 999999999);
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
    int selected = 0;

    while(1) {
        draw_main_menu(selected);
        int key = getkey().key;

        if(key == KEY_EXIT) break;

        if(key == KEY_UP && selected > 0) {
            selected--;
        }
        else if(key == KEY_DOWN && selected < 1) {
            selected++;
        }
        else if(key == KEY_EXE) {
            input_screen(selected);
        }
        else if(key == KEY_F1) {
            input_screen(0);
        }
        else if(key == KEY_F2) {
            input_screen(1);
        }
    }

    return 1;
}
