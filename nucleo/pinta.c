/* pinta: ver pinta.h */
#include "pinta.h"
#include "atlas.h"
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static int W, H;
static int tandas;

color_t color_hex(uint32_t rgb, float alfa)
{
    color_t c = {((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, alfa};
    return c;
}

/* ---------------- shaders ---------------- */

static const char *VS_CAJA =
    "uniform vec2 u_pant;\n"
    "attribute vec2 a_pos; attribute vec2 a_local; attribute vec4 a_color; attribute vec4 a_param;\n"
    "varying vec2 v_local; varying vec4 v_color; varying vec4 v_param;\n"
    "void main(){\n"
    "  gl_Position = vec4(a_pos.x / u_pant.x * 2.0 - 1.0, 1.0 - a_pos.y / u_pant.y * 2.0, 0.0, 1.0);\n"
    "  v_local = a_local; v_color = a_color; v_param = a_param; }\n";
/* distancia con signo a una caja redondeada: <0 dentro, >0 fuera */
static const char *FS_CAJA =
    "precision highp float;\n"
    "varying vec2 v_local; varying vec4 v_color; varying vec4 v_param;\n"
    "void main(){\n"
    "  vec2 q = abs(v_local) - v_param.xy + v_param.z;\n"
    "  float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - v_param.z;\n"
    "  float s = max(v_param.w, 0.6);\n"
    "  float a = 1.0 - smoothstep(-s, s, d);\n"
    "  gl_FragColor = vec4(v_color.rgb, v_color.a * a); }\n";

static const char *VS_TEXTO =
    "uniform vec2 u_pant;\n"
    "attribute vec2 a_pos; attribute vec2 a_uv; attribute vec4 a_color;\n"
    "varying vec2 v_uv; varying vec4 v_color;\n"
    "void main(){\n"
    "  gl_Position = vec4(a_pos.x / u_pant.x * 2.0 - 1.0, 1.0 - a_pos.y / u_pant.y * 2.0, 0.0, 1.0);\n"
    "  v_uv = a_uv; v_color = a_color; }\n";
static const char *FS_TEXTO =
    "precision mediump float;\n"
    "uniform sampler2D u_atlas;\n"
    "varying vec2 v_uv; varying vec4 v_color;\n"
    "void main(){ gl_FragColor = vec4(v_color.rgb, v_color.a * texture2D(u_atlas, v_uv).a); }\n";

/* imagenes: textura RGBA (alfa premultiplicado no) por el color, que tine y da alfa */
static const char *FS_IMAGEN =
    "precision mediump float;\n"
    "uniform sampler2D u_atlas;\n"
    "varying vec2 v_uv; varying vec4 v_color;\n"
    "void main(){ gl_FragColor = texture2D(u_atlas, v_uv) * v_color; }\n";

static GLuint compila(GLenum tipo, const char *src)
{
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &src, 0);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char l[512];
        glGetShaderInfoLog(s, sizeof l, 0, l);
        fprintf(stderr, "nora: shader: %s\n", l);
    }
    return s;
}
static GLuint programa(const char *vs, const char *fs, const char **atributos, int n)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, compila(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compila(GL_FRAGMENT_SHADER, fs));
    for (int i = 0; i < n; i++) glBindAttribLocation(p, i, atributos[i]);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char l[512];
        glGetProgramInfoLog(p, sizeof l, 0, l);
        fprintf(stderr, "nora: programa: %s\n", l);
    }
    return p;
}

/* ---------------- tandas ---------------- */

#define MAXQ 2048
enum { NADA, CAJAS, TEXTOS, IMAGENES };
static int modo = NADA;
static float vcaja[MAXQ * 6 * 12];   /* pos2 local2 color4 param4 */
static float vtexto[MAXQ * 6 * 8];   /* pos2 uv2 color4 */
static int nq;
static GLuint p_caja, p_texto, p_imagen, tex_atlas, tex_blanca, tex_actual;
static GLint u_caja_pant, u_texto_pant, u_texto_atlas, u_imagen_pant, u_imagen_tex;

static void manda(void)
{
    if (!nq) return;
    if (modo == CAJAS) {
        glUseProgram(p_caja);
        glUniform2f(u_caja_pant, W, H);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 48, vcaja);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 48, vcaja + 2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 48, vcaja + 4);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 48, vcaja + 8);
        glEnableVertexAttribArray(3);
    } else if (modo == IMAGENES) {
        glUseProgram(p_imagen);
        glUniform2f(u_imagen_pant, W, H);
        glUniform1i(u_imagen_tex, 0);
        glBindTexture(GL_TEXTURE_2D, tex_actual);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 32, vtexto);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, vtexto + 2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 32, vtexto + 4);
        glDisableVertexAttribArray(3);
    } else {
        glUseProgram(p_texto);
        glUniform2f(u_texto_pant, W, H);
        glUniform1i(u_texto_atlas, 0);
        glBindTexture(GL_TEXTURE_2D, tex_atlas);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 32, vtexto);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 32, vtexto + 2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 32, vtexto + 4);
        glDisableVertexAttribArray(3);
    }
    glDrawArrays(GL_TRIANGLES, 0, nq * 6);
    nq = 0;
    tandas++;
}
static void cambia(int m)
{
    if (modo != m || nq >= MAXQ) manda();
    modo = m;
}

int pinta_iniciar(int ancho, int alto)
{
    W = ancho;
    H = alto;
    const char *ac[] = {"a_pos", "a_local", "a_color", "a_param"};
    const char *at[] = {"a_pos", "a_uv", "a_color"};
    p_caja = programa(VS_CAJA, FS_CAJA, ac, 4);
    p_texto = programa(VS_TEXTO, FS_TEXTO, at, 3);
    u_caja_pant = glGetUniformLocation(p_caja, "u_pant");
    u_texto_pant = glGetUniformLocation(p_texto, "u_pant");
    u_texto_atlas = glGetUniformLocation(p_texto, "u_atlas");
    p_imagen = programa(VS_TEXTO, FS_IMAGEN, at, 3);
    u_imagen_pant = glGetUniformLocation(p_imagen, "u_pant");
    u_imagen_tex = glGetUniformLocation(p_imagen, "u_atlas");
    static const unsigned char blanco[4] = {255, 255, 255, 255};
    tex_blanca = pinta_textura(blanco, 1, 1, 1);
    glGenTextures(1, &tex_atlas);
    glBindTexture(GL_TEXTURE_2D, tex_atlas);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_W, ATLAS_H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, ATLAS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, W, H);
    return glGetError() == GL_NO_ERROR ? 0 : -1;
}

void pinta_empezar(color_t f)
{
    tandas = 0;
    pinta_recorte(0, 0, -1, -1);
    glClearColor(f.r, f.g, f.b, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void pinta_caja(float x, float y, float w, float h, float radio, color_t c, float dif)
{
    if (w <= 0 || h <= 0 || c.a <= 0) return;
    cambia(CAJAS);
    float hw = w / 2, hh = h / 2;
    if (radio > hw) radio = hw;
    if (radio > hh) radio = hh;
    if (radio < 0) radio = 0;
    float m = dif + 1;                      /* el quad crece para que quepa el borde suave */
    float cx = x + hw, cy = y + hh;
    float x0 = x - m, y0 = y - m, x1 = x + w + m, y1 = y + h + m;
    float esq[6][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y0}, {x1, y1}, {x0, y1}};
    float *v = vcaja + nq * 6 * 12;
    for (int i = 0; i < 6; i++, v += 12) {
        v[0] = esq[i][0]; v[1] = esq[i][1];
        v[2] = esq[i][0] - cx; v[3] = esq[i][1] - cy;
        v[4] = c.r; v[5] = c.g; v[6] = c.b; v[7] = c.a;
        v[8] = hw; v[9] = hh; v[10] = radio; v[11] = dif;
    }
    nq++;
}

/* UTF-8 -> Latin-1, que es lo que tiene el atlas. Los signos de fuera que mas
 * se usan se cambian por su pariente de dentro («…» por «...», «—» por «-»…);
 * el resto sale como '?'. Devuelve cuantos bytes deja en b (cabe en n). */
static int a_latin1(const char *t, unsigned char *b, int n)
{
    const unsigned char *s = (const unsigned char *)t;
    int k = 0;
    while (*s && k < n - 3) {
        int c = *s++;
        if (c >= 0x80) {
            int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
            int cp = c & (0x3f >> extra);
            for (int i = 0; i < extra && (*s & 0xc0) == 0x80; i++) cp = (cp << 6) | (*s++ & 0x3f);
            switch (cp) {
            case 0x2026: b[k++] = '.'; b[k++] = '.'; c = '.'; break;           /* … */
            case 0x2013: case 0x2014: case 0x2212: c = '-'; break;              /* – — − */
            case 0x2018: case 0x2019: c = '\''; break;                           /* ‘ ’ */
            case 0x201c: case 0x201d: c = '"'; break;                           /* “ ” */
            case 0x2022: case 0x2219: c = 0xb7; break;                          /* • → · */
            case 0x00a0: case 0x202f: c = ' '; break;
            default: c = cp < 256 ? cp : '?';
            }
        }
        b[k++] = (unsigned char)c;
    }
    b[k] = 0;
    return k;
}

float pinta_ancho(int f, const char *t)
{
    if (f < 0 || f >= ATLAS_FUENTES || !t) return 0;
    unsigned char b[1024];
    int n = a_latin1(t, b, sizeof b);
    float w = 0;
    for (int i = 0; i < n; i++) w += GLIFOS[f][b[i]].adv;
    return w;
}

float pinta_texto(int f, float x, float y, const char *t, color_t c)
{
    if (f < 0 || f >= ATLAS_FUENTES || !t) return 0;
    unsigned char b[1024];
    int n = a_latin1(t, b, sizeof b);
    float x0 = x;
    for (int k = 0; k < n; k++) {
        const glifo_t *g = &GLIFOS[f][b[k]];
        if (g->w) {
            cambia(TEXTOS);
            float gx = (float)(int)(x + g->x + 0.5f), gy = (float)(int)(y + g->y + 0.5f);   /* pixel exacto */
            float u0 = (float)g->u / ATLAS_W, v0 = (float)g->v / ATLAS_H;
            float u1 = (float)(g->u + g->w) / ATLAS_W, v1 = (float)(g->v + g->h) / ATLAS_H;
            float q[6][4] = {{gx, gy, u0, v0}, {gx + g->w, gy, u1, v0}, {gx + g->w, gy + g->h, u1, v1},
                             {gx, gy, u0, v0}, {gx + g->w, gy + g->h, u1, v1}, {gx, gy + g->h, u0, v1}};
            float *v = vtexto + nq * 6 * 8;
            for (int i = 0; i < 6; i++, v += 8) {
                v[0] = q[i][0]; v[1] = q[i][1]; v[2] = q[i][2]; v[3] = q[i][3];
                v[4] = c.r; v[5] = c.g; v[6] = c.b; v[7] = c.a;
            }
            nq++;
        }
        x += g->adv;
    }
    return x - x0;
}

/* un icono de la fuente de iconos, centrado en (cx, cy); -1 si no existe */
int pinta_icono(const char *nombre, float cx, float cy, color_t c)
{
    for (int i = 0; i < NICONOS; i++) {
        if (strcmp(nombre, ICONOS[i])) continue;
        const glifo_t *g = &GLIFOS[FUENTE_ICONOS][i];
        if (!g->w) return 0;
        cambia(TEXTOS);
        float gx = (float)(int)(cx - g->w / 2.0f + 0.5f), gy = (float)(int)(cy - g->h / 2.0f + 0.5f);
        float u0 = (float)g->u / ATLAS_W, v0 = (float)g->v / ATLAS_H;
        float u1 = (float)(g->u + g->w) / ATLAS_W, v1 = (float)(g->v + g->h) / ATLAS_H;
        float q[6][4] = {{gx, gy, u0, v0}, {gx + g->w, gy, u1, v0}, {gx + g->w, gy + g->h, u1, v1},
                         {gx, gy, u0, v0}, {gx + g->w, gy + g->h, u1, v1}, {gx, gy + g->h, u0, v1}};
        float *v = vtexto + nq * 6 * 8;
        for (int k = 0; k < 6; k++, v += 8) {
            v[0] = q[k][0]; v[1] = q[k][1]; v[2] = q[k][2]; v[3] = q[k][3];
            v[4] = c.r; v[5] = c.g; v[6] = c.b; v[7] = c.a;
        }
        nq++;
        return 0;
    }
    return -1;
}

int pinta_alto_fuente(int f) { return (f >= 0 && f < ATLAS_FUENTES) ? FUENTES[f].alto : 0; }
int pinta_fuente(const char *n)
{
    for (int i = 0; i < ATLAS_FUENTES; i++)
        if (n && !strcmp(n, FUENTES[i].nombre)) return i;
    return -1;
}
void pinta_terminar(void) { manda(); modo = NADA; }
int pinta_dibujados(void) { return tandas; }

/* ---------------- imagenes ---------------- */
unsigned pinta_textura(const unsigned char *rgba, int w, int h, int pixelado)
{
    GLuint t;
    manda();
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    GLint f = pixelado ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}
void pinta_textura_borrar(unsigned t)
{
    GLuint g = t;
    if (tex_actual == g) { manda(); tex_actual = 0; }
    glDeleteTextures(1, &g);
}
/* un cuadrilatero texturizado: esquinas en pantalla (sentido horario desde arriba
 * a la izquierda) y sus coordenadas de textura (0..1) */
void pinta_quad(unsigned tex, const float xy[8], const float uv[8], color_t c)
{
    if (c.a <= 0) return;
    if (modo != IMAGENES || tex_actual != tex || nq >= MAXQ) manda();
    modo = IMAGENES;
    tex_actual = tex;
    static const int orden[6] = {0, 1, 2, 0, 2, 3};
    float *v = vtexto + nq * 6 * 8;
    for (int i = 0; i < 6; i++, v += 8) {
        int k = orden[i];
        v[0] = xy[2 * k]; v[1] = xy[2 * k + 1]; v[2] = uv[2 * k]; v[3] = uv[2 * k + 1];
        v[4] = c.r; v[5] = c.g; v[6] = c.b; v[7] = c.a;
    }
    nq++;
}
void pinta_linea(float x1, float y1, float x2, float y2, float grosor, color_t c)
{
    float dx = x2 - x1, dy = y2 - y1, l = sqrtf(dx * dx + dy * dy);
    if (l <= 0) return;
    float nx = -dy / l * grosor / 2, ny = dx / l * grosor / 2;
    float xy[8] = {x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny};
    static const float uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    pinta_quad(tex_blanca, xy, uv, c);
}
void pinta_recorte(int x, int y, int w, int h)
{
    manda();
    if (w < 0) { glDisable(GL_SCISSOR_TEST); return; }
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, H - y - h, w, h);       /* GL cuenta desde abajo */
}
