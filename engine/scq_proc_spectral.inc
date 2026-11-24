/* scq_proc_spectral: place code (included by scq_places.c)
 *
 * Deep processing: spectral places (one shared radix-2 real FFT, Hann overlap-add), partitioned
 * convolution, and the space and low-end places that sit beside them (binaural, ambi, crossfeed,
 * bassman).
 *
 * The spectral kit. A spectral place keeps everything in its memory: a header (spx), an input
 * ring of N + H samples, work buffers of N floats (N/2 complex), an output ring of N + H, and its
 * own arrays. Each hop (H samples) a frame is taken and worked through a list of steps (window,
 * bit reversal, butterflies, real split, the place's own bin stages, back again, overlap-add). The
 * steps are cut into units of known cost, and every block does the same share of them, so the
 * frame's work is spread evenly over the hop: no block pays for a whole FFT. The output of a
 * frame starts one hop after the frame is taken: the latency is N + H samples (the window and
 * the hop of work). Everything follows the place's own rate (W.nb, W.sr), so samplerate=half
 * halves the cost and doubles the times in samples, not in seconds.
 */

/* cos(2 pi i / 2048), i = 0..512: the twiddles, the windows and every sine and cosine here
 * (interpolated, about 1e-6 off: -120 dB). In flash, not in the house. */
static const float SPX_COS[513] = {
	1.000000000f, 0.999995294f, 0.999981175f, 0.999957645f, 0.999924702f, 0.999882347f, 0.999830582f, 0.999769405f,
	0.999698819f, 0.999618822f, 0.999529418f, 0.999430605f, 0.999322385f, 0.999204759f, 0.999077728f, 0.998941293f,
	0.998795456f, 0.998640218f, 0.998475581f, 0.998301545f, 0.998118113f, 0.997925286f, 0.997723067f, 0.997511456f,
	0.997290457f, 0.997060070f, 0.996820299f, 0.996571146f, 0.996312612f, 0.996044701f, 0.995767414f, 0.995480755f,
	0.995184727f, 0.994879331f, 0.994564571f, 0.994240449f, 0.993906970f, 0.993564136f, 0.993211949f, 0.992850414f,
	0.992479535f, 0.992099313f, 0.991709754f, 0.991310860f, 0.990902635f, 0.990485084f, 0.990058210f, 0.989622017f,
	0.989176510f, 0.988721692f, 0.988257568f, 0.987784142f, 0.987301418f, 0.986809402f, 0.986308097f, 0.985797509f,
	0.985277642f, 0.984748502f, 0.984210092f, 0.983662419f, 0.983105487f, 0.982539302f, 0.981963869f, 0.981379193f,
	0.980785280f, 0.980182136f, 0.979569766f, 0.978948175f, 0.978317371f, 0.977677358f, 0.977028143f, 0.976369731f,
	0.975702130f, 0.975025345f, 0.974339383f, 0.973644250f, 0.972939952f, 0.972226497f, 0.971503891f, 0.970772141f,
	0.970031253f, 0.969281235f, 0.968522094f, 0.967753837f, 0.966976471f, 0.966190003f, 0.965394442f, 0.964589793f,
	0.963776066f, 0.962953267f, 0.962121404f, 0.961280486f, 0.960430519f, 0.959571513f, 0.958703475f, 0.957826413f,
	0.956940336f, 0.956045251f, 0.955141168f, 0.954228095f, 0.953306040f, 0.952375013f, 0.951435021f, 0.950486074f,
	0.949528181f, 0.948561350f, 0.947585591f, 0.946600913f, 0.945607325f, 0.944604837f, 0.943593458f, 0.942573198f,
	0.941544065f, 0.940506071f, 0.939459224f, 0.938403534f, 0.937339012f, 0.936265667f, 0.935183510f, 0.934092550f,
	0.932992799f, 0.931884266f, 0.930766961f, 0.929640896f, 0.928506080f, 0.927362526f, 0.926210242f, 0.925049241f,
	0.923879533f, 0.922701128f, 0.921514039f, 0.920318277f, 0.919113852f, 0.917900776f, 0.916679060f, 0.915448716f,
	0.914209756f, 0.912962190f, 0.911706032f, 0.910441292f, 0.909167983f, 0.907886116f, 0.906595705f, 0.905296759f,
	0.903989293f, 0.902673318f, 0.901348847f, 0.900015892f, 0.898674466f, 0.897324581f, 0.895966250f, 0.894599486f,
	0.893224301f, 0.891840709f, 0.890448723f, 0.889048356f, 0.887639620f, 0.886222530f, 0.884797098f, 0.883363339f,
	0.881921264f, 0.880470889f, 0.879012226f, 0.877545290f, 0.876070094f, 0.874586652f, 0.873094978f, 0.871595087f,
	0.870086991f, 0.868570706f, 0.867046246f, 0.865513624f, 0.863972856f, 0.862423956f, 0.860866939f, 0.859301818f,
	0.857728610f, 0.856147328f, 0.854557988f, 0.852960605f, 0.851355193f, 0.849741768f, 0.848120345f, 0.846490939f,
	0.844853565f, 0.843208240f, 0.841554977f, 0.839893794f, 0.838224706f, 0.836547727f, 0.834862875f, 0.833170165f,
	0.831469612f, 0.829761234f, 0.828045045f, 0.826321063f, 0.824589303f, 0.822849781f, 0.821102515f, 0.819347520f,
	0.817584813f, 0.815814411f, 0.814036330f, 0.812250587f, 0.810457198f, 0.808656182f, 0.806847554f, 0.805031331f,
	0.803207531f, 0.801376172f, 0.799537269f, 0.797690841f, 0.795836905f, 0.793975478f, 0.792106577f, 0.790230221f,
	0.788346428f, 0.786455214f, 0.784556597f, 0.782650596f, 0.780737229f, 0.778816512f, 0.776888466f, 0.774953107f,
	0.773010453f, 0.771060524f, 0.769103338f, 0.767138912f, 0.765167266f, 0.763188417f, 0.761202385f, 0.759209189f,
	0.757208847f, 0.755201377f, 0.753186799f, 0.751165132f, 0.749136395f, 0.747100606f, 0.745057785f, 0.743007952f,
	0.740951125f, 0.738887324f, 0.736816569f, 0.734738878f, 0.732654272f, 0.730562769f, 0.728464390f, 0.726359155f,
	0.724247083f, 0.722128194f, 0.720002508f, 0.717870045f, 0.715730825f, 0.713584869f, 0.711432196f, 0.709272826f,
	0.707106781f, 0.704934080f, 0.702754744f, 0.700568794f, 0.698376249f, 0.696177131f, 0.693971461f, 0.691759258f,
	0.689540545f, 0.687315341f, 0.685083668f, 0.682845546f, 0.680600998f, 0.678350043f, 0.676092704f, 0.673829000f,
	0.671558955f, 0.669282588f, 0.666999922f, 0.664710978f, 0.662415778f, 0.660114342f, 0.657806693f, 0.655492853f,
	0.653172843f, 0.650846685f, 0.648514401f, 0.646176013f, 0.643831543f, 0.641481013f, 0.639124445f, 0.636761861f,
	0.634393284f, 0.632018736f, 0.629638239f, 0.627251815f, 0.624859488f, 0.622461279f, 0.620057212f, 0.617647308f,
	0.615231591f, 0.612810082f, 0.610382806f, 0.607949785f, 0.605511041f, 0.603066599f, 0.600616479f, 0.598160707f,
	0.595699304f, 0.593232295f, 0.590759702f, 0.588281548f, 0.585797857f, 0.583308653f, 0.580813958f, 0.578313796f,
	0.575808191f, 0.573297167f, 0.570780746f, 0.568258953f, 0.565731811f, 0.563199344f, 0.560661576f, 0.558118531f,
	0.555570233f, 0.553016706f, 0.550457973f, 0.547894059f, 0.545324988f, 0.542750785f, 0.540171473f, 0.537587076f,
	0.534997620f, 0.532403128f, 0.529803625f, 0.527199135f, 0.524589683f, 0.521975293f, 0.519355990f, 0.516731799f,
	0.514102744f, 0.511468850f, 0.508830143f, 0.506186645f, 0.503538384f, 0.500885383f, 0.498227667f, 0.495565262f,
	0.492898192f, 0.490226483f, 0.487550160f, 0.484869248f, 0.482183772f, 0.479493758f, 0.476799230f, 0.474100215f,
	0.471396737f, 0.468688822f, 0.465976496f, 0.463259784f, 0.460538711f, 0.457813304f, 0.455083587f, 0.452349587f,
	0.449611330f, 0.446868840f, 0.444122145f, 0.441371269f, 0.438616239f, 0.435857080f, 0.433093819f, 0.430326481f,
	0.427555093f, 0.424779681f, 0.422000271f, 0.419216888f, 0.416429560f, 0.413638312f, 0.410843171f, 0.408044163f,
	0.405241314f, 0.402434651f, 0.399624200f, 0.396809987f, 0.393992040f, 0.391170384f, 0.388345047f, 0.385516054f,
	0.382683432f, 0.379847209f, 0.377007410f, 0.374164063f, 0.371317194f, 0.368466830f, 0.365612998f, 0.362755724f,
	0.359895037f, 0.357030961f, 0.354163525f, 0.351292756f, 0.348418680f, 0.345541325f, 0.342660717f, 0.339776884f,
	0.336889853f, 0.333999651f, 0.331106306f, 0.328209844f, 0.325310292f, 0.322407679f, 0.319502031f, 0.316593376f,
	0.313681740f, 0.310767153f, 0.307849640f, 0.304929230f, 0.302005949f, 0.299079826f, 0.296150888f, 0.293219163f,
	0.290284677f, 0.287347460f, 0.284407537f, 0.281464938f, 0.278519689f, 0.275571819f, 0.272621355f, 0.269668326f,
	0.266712757f, 0.263754679f, 0.260794118f, 0.257831102f, 0.254865660f, 0.251897818f, 0.248927606f, 0.245955050f,
	0.242980180f, 0.240003022f, 0.237023606f, 0.234041959f, 0.231058108f, 0.228072083f, 0.225083911f, 0.222093621f,
	0.219101240f, 0.216106797f, 0.213110320f, 0.210111837f, 0.207111376f, 0.204108966f, 0.201104635f, 0.198098411f,
	0.195090322f, 0.192080397f, 0.189068664f, 0.186055152f, 0.183039888f, 0.180022901f, 0.177004220f, 0.173983873f,
	0.170961889f, 0.167938295f, 0.164913120f, 0.161886394f, 0.158858143f, 0.155828398f, 0.152797185f, 0.149764535f,
	0.146730474f, 0.143695033f, 0.140658239f, 0.137620122f, 0.134580709f, 0.131540029f, 0.128498111f, 0.125454983f,
	0.122410675f, 0.119365215f, 0.116318631f, 0.113270952f, 0.110222207f, 0.107172425f, 0.104121634f, 0.101069863f,
	0.098017140f, 0.094963495f, 0.091908956f, 0.088853553f, 0.085797312f, 0.082740265f, 0.079682438f, 0.076623861f,
	0.073564564f, 0.070504573f, 0.067443920f, 0.064382631f, 0.061320736f, 0.058258265f, 0.055195244f, 0.052131705f,
	0.049067674f, 0.046003182f, 0.042938257f, 0.039872928f, 0.036807223f, 0.033741172f, 0.030674803f, 0.027608146f,
	0.024541229f, 0.021474080f, 0.018406730f, 0.015339206f, 0.012271538f, 0.009203755f, 0.006135885f, 0.003067957f,
	0.000000000f
};

static inline float spx_ct(int i)          /* cos(2 pi i / 2048) for any i */
{
	i &= 2047;
	if (i <= 512) return SPX_COS[i];
	if (i <= 1024) return -SPX_COS[1024 - i];
	if (i <= 1536) return -SPX_COS[i - 1024];
	return SPX_COS[2048 - i];
}
static inline float spx_st(int i) { return spx_ct(i - 512); }   /* sin(2 pi i / 2048) */

static inline float spx_cos(float p)       /* cos(2 pi p), p in cycles */
{
	p -= scq_floor(p);
	float x = p * 2048.f;
	int i = (int)x;
	float f = x - (float)i, a = spx_ct(i);
	return a + (spx_ct(i + 1) - a) * f;
}
static inline float spx_sin(float p) { return spx_cos(p - 0.25f); }

/* atan2 in cycles (-0.5 .. 0.5), about 1e-8 off (Abramowitz & Stegun 4.4.49) */
static inline float spx_atan2(float y, float x)
{
	float ax = scq_abs(x), ay = scq_abs(y);
	if (!(ax + ay > 1e-30f)) return 0.f;
	int sw = ay > ax;
	float t = sw ? ax / ay : ay / ax, t2 = t * t;
	float a = t * (0.9999993329f + t2 * (-0.3332985605f + t2 * (0.1994653599f + t2 * (-0.1390853351f
	        + t2 * (0.0964200441f + t2 * (-0.0559098861f + t2 * (0.0218612288f + t2 * -0.0040540580f)))))));
	if (sw) a = 1.57079633f - a;
	if (x < 0.f) a = 3.14159265f - a;
	if (y < 0.f) a = -a;
	return a * 0.159154943f;
}

static inline float spx_sqrt(float x)
{
	if (!(x > 1e-30f)) return 0.f;
	if (x > 1e30f) x = 1e30f;
	union { float f; uint32_t i; } u;
	u.f = x;
	u.i = 0x5f3759dfu - (u.i >> 1);
	float y = u.f;
	y = y * (1.5f - 0.5f * x * y * y);
	y = y * (1.5f - 0.5f * x * y * y);
	y = y * (1.5f - 0.5f * x * y * y);
	return x * y;
}

/* log2(x), about 1e-6 off (scq_log2 is a quick sketch, a whole octave out) */
static inline float spx_log2(float x)
{
	if (!(x > 1e-30f)) return -100.f;
	if (x > 1e30f) x = 1e30f;
	union { float f; uint32_t i; } u;
	u.f = x;
	int e = (int)((u.i >> 23) & 255u) - 127;
	u.i = (u.i & 0x007fffffu) | 0x3f800000u;      /* 1 .. 2 */
	float m = u.f;
	if (m > 1.41421356f) { m *= 0.5f; e++; }
	float t = (m - 1.f) / (m + 1.f), t2 = t * t;
	return (float)e + 2.885390082f * t * (1.f + t2 * (0.333333333f + t2 * (0.2f + t2 * (0.142857143f + t2 * 0.111111111f))));
}

static inline float spx_ok(float x, float lim) { return x == x ? scq_clamp(x, -lim, lim) : 0.f; }
static inline float spx_wrap(float p) { return p - scq_floor(p + 0.5f); }   /* to -0.5 .. 0.5 */

static inline int spx_rev(int i, int bits)
{
	uint32_t x = (uint32_t)i;
	x = ((x & 0x5555u) << 1) | ((x >> 1) & 0x5555u);
	x = ((x & 0x3333u) << 2) | ((x >> 2) & 0x3333u);
	x = ((x & 0x0f0fu) << 4) | ((x >> 4) & 0x0f0fu);
	x = ((x & 0x00ffu) << 8) | ((x >> 8) & 0x00ffu);
	return (int)(x >> (16 - bits));
}

/* hashed 0..1 for bin k (the random curves) */
static inline float spx_hash01(uint32_t k, uint32_t seed)
{
	uint32_t x = k * 0x9e3779b1u ^ seed;
	x ^= x >> 15; x *= 0x85ebca6bu; x ^= x >> 13; x *= 0xc2b2ae35u; x ^= x >> 16;
	return (float)(x >> 8) * (1.f / 16777216.f);
}

typedef struct {
	int32_t N, n2, lg2, H, R, win, nring, nout;
	int32_t w, r, fill, fend, ostart;
	int32_t op, unit, busy, frames, xunits;
	float   credit, rate, xcost, pad;
	int32_t ring[2], work[3], out[2], x[14];
} spx;
#define SPX_HDR ((int32_t)((sizeof(spx) + 15u) / 16u * 4u))   /* in floats */

enum { OP_END, OP_IN, OP_REV, OP_FFT, OP_IFFT, OP_SPLIT, OP_UNSPLIT, OP_BINS, OP_OLA, OP_SAVE };
enum { SPX_HANN, SPX_SINE, SPX_RECT };   /* hann/hann at 75%, sine/sine at 50%, rect (overlap-save) */
/* a step: kind, a (buffer or stage), b (ring, or for OP_BINS: 0 = n2 units, 1 = xunits, 2 = one),
 * cost in M4F cycles per unit (0: the header's xcost) */
typedef struct { uint8_t kind, a, b, cost; } spx_op;
typedef void (*spx_fn)(node *n, spx *h, int stage, int k0, int k1);

static inline spx *spx_h(node *n) { return (spx *)n->mem; }
static inline float *spx_m(node *n, int32_t off) { return (float *)n->mem + off; }

/* lay out the memory: header, rings, work buffers, output rings, the place's arrays (xs[k] floats).
 * With h = 0 it only counts the bytes (the mem function). */
static uint32_t spx_lay(spx *h, int N, int H, int win, int nring, int nwork, int nout, const int32_t *xs, int nx)
{
	int32_t o = SPX_HDR, R = N + H;
	int lg = 0;
	while ((1 << lg) < N / 2) lg++;
	if (h) {
		h->N = N; h->n2 = N / 2; h->lg2 = lg; h->H = H; h->R = R; h->win = win;
		h->nring = nring; h->nout = nout;
	}
	for (int k = 0; k < nring; k++) { if (h) h->ring[k] = o; o += R; }
	for (int k = 0; k < nwork; k++) { if (h) h->work[k] = o; o += N; }
	for (int k = 0; k < nout; k++) { if (h) h->out[k] = o; o += R; }
	for (int k = 0; k < nx; k++) { if (h) h->x[k] = o; o += (xs[k] + 3) & ~3; }
	return (uint32_t)o * 4u;
}

static int spx_units(const spx *h, const spx_op *op)
{
	switch (op->kind) {
	case OP_IN: case OP_REV: case OP_OLA: return h->n2;
	case OP_FFT: case OP_IFFT: return h->lg2 * (h->n2 >> 1);
	case OP_SPLIT: case OP_UNSPLIT: return (h->n2 >> 1) + 1;
	case OP_SAVE: return h->n2 >> 1;
	case OP_BINS: return op->b == 0 ? h->n2 : (op->b == 1 ? h->xunits : 1);
	default: return 0;
	}
}
static inline float spx_cost(const spx *h, const spx_op *op) { float c = op->cost ? (float)op->cost : h->xcost; return c < 1.f ? 1.f : c; }

/* the cost of a frame spread over its hop, per sample (+5% so it always ends in time) */
static void spx_rate(spx *h, const spx_op *ops)
{
	float t = 0.f;
	for (const spx_op *op = ops; op->kind != OP_END; op++) t += (float)spx_units(h, op) * spx_cost(h, op);
	h->rate = t * 1.05f / (float)h->H + 1.f;
}

static void spx_win_in(const spx *h, float *z, const float *ring, int u0, int u1)
{
	const int N = h->N, R = h->R;
	int st = h->fend - N;
	if (st < 0) st += R;
	for (int k = u0; k < u1; k++) {
		int j = 2 * k, a = st + j, b;
		if (a >= R) a -= R;
		b = a + 1 < R ? a + 1 : 0;
		float w0 = 1.f, w1 = 1.f;
		if (h->win == SPX_HANN) { const int t = 2048 / N; w0 = 0.5f - 0.5f * spx_ct(j * t); w1 = 0.5f - 0.5f * spx_ct((j + 1) * t); }
		else if (h->win == SPX_SINE) { const int t = 1024 / N; w0 = spx_st(j * t); w1 = spx_st((j + 1) * t); }
		z[j] = ring[a] * w0; z[j + 1] = ring[b] * w1;
	}
}

static void spx_bitrev(const spx *h, float *z, int u0, int u1)
{
	for (int i = u0; i < u1; i++) {
		int j = spx_rev(i, h->lg2);
		if (i < j) {
			float t = z[2 * i]; z[2 * i] = z[2 * j]; z[2 * j] = t;
			t = z[2 * i + 1]; z[2 * i + 1] = z[2 * j + 1]; z[2 * j + 1] = t;
		}
	}
}

/* radix-2 decimation in time, after the bit reversal; unit u is one butterfly of pass u / (n2/2) */
static void spx_fft(const spx *h, float *z, int u0, int u1, int inv)
{
	const int lgh = h->lg2 - 1, half = h->n2 >> 1;
	for (int u = u0; u < u1; u++) {
		int s = u >> lgh, j = u & (half - 1);
		int m = 1 << s, k = j & (m - 1);
		int i0 = ((j >> s) << (s + 1)) + k, i1 = i0 + m;
		int ti = k << (10 - s);
		float c = spx_ct(ti), sn = inv ? spx_st(ti) : -spx_st(ti);
		float *a = z + 2 * i0, *b = z + 2 * i1;
		float tr = c * b[0] - sn * b[1], tq = c * b[1] + sn * b[0];
		b[0] = a[0] - tr; b[1] = a[1] - tq;
		a[0] += tr; a[1] += tq;
	}
}

/* N/2-point complex FFT of the packed real frame -> the half spectrum X[0..N/2-1], with the
 * Nyquist bin's (real) value in X[0]'s imaginary part */
static void spx_split(const spx *h, float *z, int u0, int u1)
{
	const int n2 = h->n2, t = 2048 / h->N;
	for (int k = u0; k < u1; k++) {
		if (k == 0) { float re = z[0], im = z[1]; z[0] = re + im; z[1] = re - im; continue; }
		int c = n2 - k;
		float zr = z[2 * k], zi = z[2 * k + 1], cr = z[2 * c], ci = -z[2 * c + 1];
		float er = 0.5f * (zr + cr), ei = 0.5f * (zi + ci);
		float orr = 0.5f * (zi - ci), oi = -0.5f * (zr - cr);
		float wc = spx_ct(k * t), ws = -spx_st(k * t);
		float pr = wc * orr - ws * oi, pi = wc * oi + ws * orr;
		z[2 * k] = er + pr; z[2 * k + 1] = ei + pi;
		if (c != k) { z[2 * c] = er - pr; z[2 * c + 1] = pi - ei; }
	}
}

static void spx_unsplit(const spx *h, float *z, int u0, int u1)
{
	const int n2 = h->n2, t = 2048 / h->N;
	for (int k = u0; k < u1; k++) {
		if (k == 0) { float dc = z[0], ny = z[1]; z[0] = 0.5f * (dc + ny); z[1] = 0.5f * (dc - ny); continue; }
		int c = n2 - k;
		float xr = z[2 * k], xi = z[2 * k + 1], cr = z[2 * c], ci = -z[2 * c + 1];
		float er = 0.5f * (xr + cr), ei = 0.5f * (xi + ci);
		float dr = 0.5f * (xr - cr), di = 0.5f * (xi - ci);
		float wc = spx_ct(k * t), ws = spx_st(k * t);
		float orr = dr * wc - di * ws, oi = dr * ws + di * wc;
		z[2 * k] = er - oi; z[2 * k + 1] = ei + orr;
		if (c != k) { z[2 * c] = er + oi; z[2 * c + 1] = orr - ei; }
	}
}

static void spx_ola(const spx *h, const float *z, float *out, int u0, int u1)
{
	const int N = h->N, R = h->R;
	const float sc = h->win == SPX_HANN ? 1.f / (1.5f * (float)h->n2) : 1.f / (float)h->n2;
	for (int k = u0; k < u1; k++) {
		int j = 2 * k, a = h->ostart + j, b;
		if (a >= R) a -= R;
		b = a + 1 < R ? a + 1 : 0;
		float w0 = sc, w1 = sc;
		if (h->win == SPX_HANN) { const int t = 2048 / N; w0 *= 0.5f - 0.5f * spx_ct(j * t); w1 *= 0.5f - 0.5f * spx_ct((j + 1) * t); }
		else if (h->win == SPX_SINE) { const int t = 1024 / N; w0 *= spx_st(j * t); w1 *= spx_st((j + 1) * t); }
		out[a] += z[j] * w0; out[b] += z[j + 1] * w1;
	}
}

/* overlap-save: the second half of the frame is this hop's output */
static void spx_save(const spx *h, const float *z, float *out, int u0, int u1)
{
	const int R = h->R, n2 = h->n2;
	const float sc = 1.f / (float)n2;
	for (int k = u0; k < u1; k++) {
		int a = h->ostart + 2 * k, b;
		if (a >= R) a -= R;
		b = a + 1 < R ? a + 1 : 0;
		out[a] = z[n2 + 2 * k] * sc; out[b] = z[n2 + 2 * k + 1] * sc;
	}
}

static void spx_exec(node *n, spx *h, const spx_op *op, int u0, int u1, spx_fn fn)
{
	switch (op->kind) {
	case OP_IN:      spx_win_in(h, spx_m(n, h->work[op->a]), spx_m(n, h->ring[op->b]), u0, u1); break;
	case OP_REV:     spx_bitrev(h, spx_m(n, h->work[op->a]), u0, u1); break;
	case OP_FFT:     spx_fft(h, spx_m(n, h->work[op->a]), u0, u1, 0); break;
	case OP_IFFT:    spx_fft(h, spx_m(n, h->work[op->a]), u0, u1, 1); break;
	case OP_SPLIT:   spx_split(h, spx_m(n, h->work[op->a]), u0, u1); break;
	case OP_UNSPLIT: spx_unsplit(h, spx_m(n, h->work[op->a]), u0, u1); break;
	case OP_BINS:    fn(n, h, op->a, u0, u1); break;
	case OP_OLA:     spx_ola(h, spx_m(n, h->work[op->a]), spx_m(n, h->out[op->b]), u0, u1); break;
	case OP_SAVE:    spx_save(h, spx_m(n, h->work[op->a]), spx_m(n, h->out[op->b]), u0, u1); break;
	default: break;
	}
}

#ifdef SPX_DEBUG
static int spx_late;
static double spx_dt[32], spx_du[32];
static double spx_clock(void);
#endif
/* work on the frame: as much as the credit buys, or all of what is left */
static void spx_work(node *n, spx *h, const spx_op *ops, spx_fn fn, int all)
{
	while (h->busy) {
		const spx_op *op = ops + h->op;
		if (op->kind == OP_END) { h->busy = 0; h->frames++; break; }
		int U = spx_units(h, op), todo = U - h->unit;
		float c = spx_cost(h, op);
		if (!all) {
			if (h->credit <= 0.f) break;
			int can = (int)(h->credit / c) + 1;
			if (can < todo) todo = can;
			h->credit -= (float)todo * c;
		}
#ifdef SPX_DEBUG
		double t0 = spx_clock();
#endif
		if (todo > 0) spx_exec(n, h, op, h->unit, h->unit + todo, fn);
#ifdef SPX_DEBUG
		spx_dt[h->op] += spx_clock() - t0; spx_du[h->op] += todo;
#endif
		h->unit += todo;
		if (h->unit >= U) { h->op++; h->unit = 0; }
	}
	if (!h->busy && h->credit > 0.f) h->credit = 0.f;   /* idle: nothing is saved up */
}


/* one block: the samples in and out, a new frame every hop, and this block's share of the work.
 * a/b feed the input rings, oa/ob take the output rings (b, ob may be 0). Returns 1 when a
 * frame was started this block. */
static int spx_step(node *n, spx *h, const spx_op *ops, spx_fn fn, const float *a, const float *b, float *oa, float *ob)
{
	float *r0 = spx_m(n, h->ring[0]), *r1 = h->nring > 1 ? spx_m(n, h->ring[1]) : 0;
	float *q0 = h->nout > 0 ? spx_m(n, h->out[0]) : 0, *q1 = h->nout > 1 ? spx_m(n, h->out[1]) : 0;
	int w = h->w, r = h->r, R = h->R, started = 0;
	LOOP {
		r0[w] = spx_ok(a[i], 4.f);
		if (r1) r1[w] = spx_ok(b ? b[i] : 0.f, 4.f);
		if (++w >= R) w = 0;
		if (q0) { oa[i] = spx_ok(q0[r], 8.f); q0[r] = 0.f; }
		if (q1) { ob[i] = spx_ok(q1[r], 8.f); q1[r] = 0.f; }
		if (++r >= R) r = 0;
	}
	h->w = w; h->r = r;
	h->fill += B;
	if (h->fill >= h->H) {
		h->fill -= h->H;
		if (h->busy) {                            /* never: the credit always covers a frame */
#ifdef SPX_DEBUG
			spx_late++;
#endif
			spx_work(n, h, ops, fn, 1);
		}
		h->fend = w;
		h->ostart = r + h->H;
		if (h->ostart >= R) h->ostart -= R;
		h->op = 0; h->unit = 0; h->busy = 1;
		started = 1;
	}
	h->credit += h->rate * (float)B;
	spx_work(n, h, ops, fn, 0);
	return started;
}

/* FFT size from the nature: 1 = 512, 2 = 1024, 3 = 2048 */
static int spx_size(float nature) { return 512 << (natural(nature, 1, 3) - 1); }

/* a pole for "about this many seconds" at a frame rate of one per hop */
static inline float spx_hopcoef(float seconds, const spx *h) { return scq_coef_time(seconds, SR / (float)h->H); }

#define SPX_FWD(wk, rg) { OP_IN, wk, rg, 24 }, { OP_REV, wk, 0, 10 }, { OP_FFT, wk, 0, 30 }, { OP_SPLIT, wk, 0, 45 }
#define SPX_BACK(wk, o) { OP_UNSPLIT, wk, 0, 40 }, { OP_REV, wk, 0, 10 }, { OP_IFFT, wk, 0, 30 }, { OP_OLA, wk, o, 24 }
#define SPX_END         { OP_END, 0, 0, 0 }

/* ------------------------------------------------------------------ specgate */
/* the fog that only lets the loud through: per-bin gate, or its inverse (keep=quiet) */
static const spx_op OPS_GATE[] = { SPX_FWD(0, 0), { OP_BINS, 0, 0, 40 }, SPX_BACK(0, 0), SPX_END };
static uint32_t gate_lay(spx *h, float nature)
{
	int N = spx_size(nature);
	int32_t xs[1] = { N / 2 };
	return spx_lay(h, N, N / 4, SPX_HANN, 1, 1, 1, xs, 1);
}
static uint32_t mem_specgate(float nature) { return gate_lay(0, nature); }
static void bins_specgate(node *n, spx *h, int stage, int k0, int k1)
{
	(void)stage;
	float *z = spx_m(n, h->work[0]), *g = spx_m(n, h->x[0]);
	const float norm = 16.f / ((float)h->N * (float)h->N);   /* a full-scale sine's bin: 1 */
	const float ithr = 1.f / n->s[2], rel = n->s[3];
	const int quiet = n->opt[0] >= 0.5f;
	for (int k = k0; k < k1; k++) {
		if (k == 0) { z[0] = z[1] = 0.f; continue; }
		float p = (z[2 * k] * z[2 * k] + z[2 * k + 1] * z[2 * k + 1]) * norm * ithr;
		p *= p;                                   /* a soft knee about 6 dB wide */
		float t = p / (1.f + p);
		if (quiet) t = 1.f - t;
		float gk = g[k];
		gk += (t - gk) * (t > gk ? 0.7f : rel);
		if (gk < 1e-6f) gk = 0.f;
		g[k] = gk;
		z[2 * k] *= gk; z[2 * k + 1] *= gk;
	}
}
static void run_specgate(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) { gate_lay(h, n->nature); spx_rate(h, OPS_GATE); }
	float db = -90.f + 90.f * scq_clamp(in[1][0], 0.f, 1.f);
	float lin = scq_exp2(db * 0.16609640f);
	n->s[2] = scq_max(lin * lin, 1e-12f);
	n->s[3] = spx_hopcoef(0.1f, h);
	spx_step(n, h, OPS_GATE, bins_specgate, in[0], 0, n->out[0], 0);
}

/* ------------------------------------------------------------------ specfilter */
static const spx_op OPS_FILT[] = { SPX_FWD(0, 0), { OP_BINS, 0, 0, 90 }, SPX_BACK(0, 0), SPX_END };
static uint32_t filt_lay(spx *h, float nature)
{
	int N = spx_size(nature);
	int32_t xs[1] = { N / 2 };
	return spx_lay(h, N, N / 4, SPX_HANN, 1, 1, 1, xs, 1);
}
static uint32_t mem_specfilter(float nature) { return filt_lay(0, nature); }
static const float SPX_VOWEL[5][3] = {   /* a e i o u: F1 F2 F3 */
	{ 800.f, 1150.f, 2900.f }, { 400.f, 1700.f, 2600.f }, { 280.f, 2250.f, 2900.f },
	{ 450.f, 800.f, 2830.f }, { 325.f, 700.f, 2530.f } };
static void bins_specfilter(node *n, spx *h, int stage, int k0, int k1)
{
	(void)stage;
	float *z = spx_m(n, h->work[0]), *g = spx_m(n, h->x[0]);
	const int shape = (int)n->opt[0];
	const float a = n->s[2], f0 = n->s[3], hz = SR / (float)h->N, if0 = 1.f / f0;
	float F[3] = { 0.f, 0.f, 0.f };
	if (shape == 3) {
		float v = a * 4.f;
		int vi = (int)v;
		if (vi > 3) vi = 3;
		float fr = v - (float)vi, sc = f0 * (1.f / 110.f);
		for (int j = 0; j < 3; j++) F[j] = (SPX_VOWEL[vi][j] + (SPX_VOWEL[vi + 1][j] - SPX_VOWEL[vi][j]) * fr) * sc;
	}
	const float bw = scq_max(1.2f * hz, 0.03f * f0);
	for (int k = k0; k < k1; k++) {
		if (k == 0) { z[0] = z[1] = 0.f; continue; }
		float f = (float)k * hz, G = 1.f;
		switch (shape) {
		case 0: {                                          /* comb: peaks on the harmonics of f0 */
			float c = 0.5f + 0.5f * spx_cos(f * if0);
			G = 1.f - a + a * c * c;
		} break;
		case 1: {                                          /* teeth: three to the octave */
			float t = 0.5f + 0.5f * spx_cos(3.f * spx_log2(f * if0));
			G = 1.f - a * scq_clamp((t - 0.5f) * 6.f + 0.5f, 0.f, 1.f);
		} break;
		case 2: {                                          /* tilt: -6 .. +6 dB an octave about f0 x 9 */
			float e = (a * 2.f - 1.f) * spx_log2(f * if0 * (1.f / 9.f));
			G = scq_exp2(scq_clamp(e, -6.f, 6.f));
		} break;
		case 3: {                                          /* formants: a e i o u */
			G = 0.05f;
			static const float AMP[3] = { 1.f, 0.6f, 0.35f }, BW[3] = { 80.f, 100.f, 130.f };
			for (int j = 0; j < 3; j++) { float d = (f - F[j]) * (1.f / BW[j]); G += AMP[j] / (1.f + d * d); }
		} break;
		default: {                                         /* harmonics: only the harmonics of f0 */
			float r = f * if0, d = scq_abs(r - scq_floor(r + 0.5f)) * f0 / bw;
			float w = r < 0.5f ? 0.f : scq_max(0.f, 1.f - d * d);
			G = 1.f - a + a * w;
		} break;
		}
		float gk = g[k] + (G - g[k]) * 0.5f;
		if (gk < 1e-6f) gk = 0.f;
		g[k] = gk;
		z[2 * k] *= gk; z[2 * k + 1] *= gk;
	}
}
static void run_specfilter(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) { filt_lay(h, n->nature); spx_rate(h, OPS_FILT); }
	n->opt[0] = scq_clamp(n->opt[0], 0.f, 4.f);
	n->s[2] = scq_clamp(in[1][0], 0.f, 1.f);
	n->s[3] = scq_clamp(110.f * scq_exp2(scq_clamp(in[2][0], -4.f, 6.f)), 20.f, 0.25f * SR);
	spx_step(n, h, OPS_FILT, bins_specfilter, in[0], 0, n->out[0], 0);
}

/* ------------------------------------------------------------------ deverb */
static const spx_op OPS_DEVERB[] = { SPX_FWD(0, 0), { OP_BINS, 0, 0, 60 }, SPX_BACK(0, 0), SPX_END };
static uint32_t deverb_lay(spx *h, float nature)
{
	int N = spx_size(nature);
	int32_t xs[2] = { N / 2, N / 2 };
	return spx_lay(h, N, N / 4, SPX_HANN, 1, 1, 1, xs, 2);
}
static uint32_t mem_deverb(float nature) { return deverb_lay(0, nature); }
static void bins_deverb(node *n, spx *h, int stage, int k0, int k1)
{
	(void)stage;
	float *z = spx_m(n, h->work[0]), *est = spx_m(n, h->x[0]), *g = spx_m(n, h->x[1]);
	const float a = n->s[2], c = n->s[3];
	for (int k = k0; k < k1; k++) {
		if (k == 0) { z[0] = z[1] = 0.f; continue; }
		float m = spx_sqrt(z[2 * k] * z[2 * k] + z[2 * k + 1] * z[2 * k + 1]);
		float e = est[k], q = scq_min(1.f, 0.85f * e / (m + 1e-9f));
		float G = a >= 0.f ? scq_max(0.03f, 1.f - a * q) : 1.f - 2.f * a * q;
		float gk = g[k] + (G - g[k]) * 0.6f;
		g[k] = gk;
		e += (m - e) * c;
		est[k] = e < 1e-12f ? 0.f : e;
		z[2 * k] *= gk; z[2 * k + 1] *= gk;
	}
}
static void run_deverb(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) { deverb_lay(h, n->nature); spx_rate(h, OPS_DEVERB); }
	n->s[2] = scq_clamp(in[1][0], -1.f, 1.f);
	n->s[3] = spx_hopcoef(0.15f, h);
	spx_step(n, h, OPS_DEVERB, bins_deverb, in[0], 0, n->out[0], 0);
}

/* ------------------------------------------------------------------ morph */
static const spx_op OPS_MORPH[] = { SPX_FWD(0, 0), SPX_FWD(1, 1), { OP_BINS, 0, 0, 30 }, { OP_BINS, 1, 0, 20 },
	{ OP_BINS, 2, 0, 20 }, { OP_BINS, 3, 0, 45 }, SPX_BACK(1, 0), SPX_END };
static uint32_t morph_lay(spx *h, float nature)
{
	int N = spx_size(nature);
	int32_t xs[2] = { N / 2, N / 2 };
	return spx_lay(h, N, N / 4, SPX_HANN, 2, 2, 1, xs, 2);
}
static uint32_t mem_morph(float nature) { return morph_lay(0, nature); }
static void bins_morph(node *n, spx *h, int stage, int k0, int k1)
{
	float *A = spx_m(n, h->work[0]), *Bz = spx_m(n, h->work[1]);
	float *ea = spx_m(n, h->x[0]), *eb = spx_m(n, h->x[1]);
	const float al = n->s[3];
	const int n2 = h->n2;
	if (stage == 0) {
		for (int k = k0; k < k1; k++) {
			if (k == 0) { ea[0] = eb[0] = 0.f; continue; }
			ea[k] = A[2 * k] * A[2 * k] + A[2 * k + 1] * A[2 * k + 1];   /* power: the envelopes are RMS, so */
			eb[k] = Bz[2 * k] * Bz[2 * k] + Bz[2 * k + 1] * Bz[2 * k + 1];   /* a sparse b keeps a's loudness */
		}
	} else if (stage == 1) {                    /* the envelopes: a pole across the bins, forwards ... */
		if (al >= 0.999f) return;
		for (int k = k0 < 2 ? 2 : k0; k < k1; k++) { ea[k] = ea[k - 1] + al * (ea[k] - ea[k - 1]); eb[k] = eb[k - 1] + al * (eb[k] - eb[k - 1]); }
	} else if (stage == 2) {                    /* ... and back: no shift */
		if (al >= 0.999f) return;
		for (int u = k0; u < k1; u++) {
			int k = n2 - 2 - u;
			if (k < 1) continue;
			ea[k] = ea[k + 1] + al * (ea[k] - ea[k + 1]); eb[k] = eb[k + 1] + al * (eb[k] - eb[k + 1]);
		}
	} else {
		const float bl = n->s[2];
		for (int k = k0; k < k1; k++) {
			if (k == 0) { Bz[0] = Bz[1] = 0.f; continue; }
			float r = scq_min(spx_sqrt(ea[k] / (eb[k] + 1e-12f)), 1000.f);
			float gk = 1.f - bl + bl * r;
			Bz[2 * k] *= gk; Bz[2 * k + 1] *= gk;
		}
	}
}
static void run_morph(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) { morph_lay(h, n->nature); spx_rate(h, OPS_MORPH); }
	n->s[2] = scq_clamp(in[2][0], 0.f, 1.f);
	float sm = scq_clamp(in[3][0], 0.f, 1.f), width = sm * sm * 3000.f * (float)h->N / SR;
	n->s[3] = 1.f / (1.f + width);
	spx_step(n, h, OPS_MORPH, bins_morph, in[0], in[1], n->out[0], 0);
}

/* ------------------------------------------------------------------ the phase vocoder (specshift, harmonizer) */
/* Analysis: magnitude, phase and true frequency of every bin; every bin then belongs to its
 * nearest peak. Synthesis moves each peak's whole region (its shape kept) to the peak's new
 * frequency, the phases locked to the peak's (Laroche & Dolson's identity phase locking): clean
 * where a plain phase vocoder goes phasey. arrays: x0 mag, x1 phase, x2 freq (bins), x3 owner. */
static void pv_bins(node *n, spx *h, int stage, int k0, int k1, int clear0, int clear1, int nwm)
{
	float *z = spx_m(n, h->work[0]);
	float *mag = spx_m(n, h->x[0]), *ph = spx_m(n, h->x[1]), *fq = spx_m(n, h->x[2]), *own = spx_m(n, h->x[3]);
	const int n2 = h->n2;
	const float adv = (float)h->H / (float)h->N, iadv = 1.f / adv, floor_ = 1e-4f;
	if (stage == 0) {
		for (int k = k0; k < k1; k++) {
			if (k == 0) { mag[0] = 0.f; continue; }
			float re = z[2 * k], im = z[2 * k + 1];
			float m = spx_sqrt(re * re + im * im), p = spx_atan2(im, re);
			float d = spx_wrap(p - ph[k] - (float)k * adv);
			fq[k] = (float)k + d * iadv;
			ph[k] = p; mag[k] = m;
		}
	} else if (stage == 1) {
		for (int k = k0; k < k1; k++) {
			float *c = spx_m(n, h->work[clear0]);
			c[2 * k] = c[2 * k + 1] = 0.f;
			if (clear1) { c = spx_m(n, h->work[clear1]); c[2 * k] = c[2 * k + 1] = 0.f; }
			for (int j = 0; j < nwm; j++) spx_m(n, h->x[4 + j])[k] = 0.f;
		}
	} else if (stage == 2) {
		if (k0 == 0) n->s[20] = -1.f;
		float last = n->s[20];
		for (int k = k0; k < k1; k++) {
			if (k >= 1 && k <= n2 - 2 && mag[k] > floor_ && mag[k] > mag[k - 1] && mag[k] >= mag[k + 1]) last = (float)k;
			own[k] = last;
		}
		n->s[20] = last;
	} else if (stage == 3) {
		if (k0 == 0) n->s[21] = -1.f;
		float nx = n->s[21];
		for (int u = k0; u < k1; u++) {
			int k = n2 - 1 - u;
			if (k >= 1 && k <= n2 - 2 && mag[k] > floor_ && mag[k] > mag[k - 1] && mag[k] >= mag[k + 1]) nx = (float)k;
			float l = own[k];
			if (l < 0.f || (nx >= 0.f && nx - (float)k < (float)k - l)) l = nx;
			own[k] = l;
		}
		n->s[21] = nx;
	}
}

/* one voice of synthesis into work buffer `out` with gain gl (and gr into out2): bins k0..k1,
 * ratio and stretch given, syn arrays old/new. The peak being worked is cached in c[0..3]. */
static void pv_voice(node *n, spx *h, int k0, int k1, float ratio, float stretch, float *old, float *nw, float *wm,
                     float *out, float gl, float *out2, float gr, float *c)
{
	const float *mag = spx_m(n, h->x[0]), *ph = spx_m(n, h->x[1]), *fq = spx_m(n, h->x[2]), *own = spx_m(n, h->x[3]);
	const int n2 = h->n2;
	const float adv = (float)h->H / (float)h->N, bhz = SR / (float)h->N;
	for (int k = k0; k < k1; k++) {
		float pf = own[k];
		if (pf < 0.f) continue;
		int p = (int)pf;
		if (pf != c[0]) {                        /* a new peak: where it goes, and its phase there */
			float f = fq[p];
			if (stretch != 0.f) {
				float hz = scq_max(f * bhz, 1.f);
				f = 100.f * scq_exp2((1.f + 0.3f * stretch) * spx_log2(hz * 0.01f)) / bhz;
			}
			f *= ratio;
			int tp = (int)(f + 0.5f);
			c[0] = pf; c[1] = (float)(tp - p);
			c[3] = (tp >= 1 && tp < n2) ? 1.f : 0.f;
			c[2] = c[3] != 0.f ? spx_wrap(old[tp] + f * adv) : 0.f;
		}
		if (c[3] == 0.f) continue;
		int t = k + (int)c[1];
		if (t < 1 || t >= n2) continue;
		float phi = spx_wrap(c[2] + ph[k] - ph[p]);
		float m = mag[k];
		if (m > wm[t]) { wm[t] = m; nw[t] = phi; }   /* the loudest that lands here leads next frame's phase */
		float co = spx_cos(phi) * m, si = spx_sin(phi) * m;
		out[2 * t] += gl * co; out[2 * t + 1] += gl * si;
		if (out2) { out2[2 * t] += gr * co; out2[2 * t + 1] += gr * si; }
	}
}

/* ------------------------------------------------------------------ specshift */
static const spx_op OPS_SHIFT[] = { SPX_FWD(0, 0), { OP_BINS, 0, 0, 75 }, { OP_BINS, 1, 0, 8 }, { OP_BINS, 2, 0, 25 },
	{ OP_BINS, 3, 0, 25 }, { OP_BINS, 4, 0, 110 }, SPX_BACK(1, 0), SPX_END };
static uint32_t shift_lay(spx *h, float nature)
{
	int N = spx_size(nature), n2 = N / 2;
	int32_t xs[7] = { n2, n2, n2, n2, n2, n2, n2 };   /* mag phase freq owner, loudest, synth phases x 2 */
	return spx_lay(h, N, N / 4, SPX_HANN, 1, 2, 1, xs, 7);
}
static uint32_t mem_specshift(float nature) { return shift_lay(0, nature); }
static void bins_specshift(node *n, spx *h, int stage, int k0, int k1)
{
	if (stage < 4) { pv_bins(n, h, stage, k0, k1, 1, 0, 1); return; }
	int par = h->frames & 1;
	float *old = spx_m(n, h->x[5 + par]), *nw = spx_m(n, h->x[6 - par]);
	if (k0 == 0) n->s[12] = -1.f;
	pv_voice(n, h, k0, k1, n->s[2], n->s[3], old, nw, spx_m(n, h->x[4]), spx_m(n, h->work[1]), 1.f, 0, 0.f, &n->s[12]);
}
static void run_specshift(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) { shift_lay(h, n->nature); spx_rate(h, OPS_SHIFT); }
	if (h->busy == 0 || h->op == 0) {           /* latched for the frame */
		n->s[2] = scq_exp2(scq_clamp(in[1][0], -3.f, 3.f));
		n->s[3] = scq_clamp(in[2][0], -1.f, 1.f);
	}
	spx_step(n, h, OPS_SHIFT, bins_specshift, in[0], 0, n->out[0], 0);
}

/* ------------------------------------------------------------------ harmonizer */
static const spx_op OPS_HARM[] = { SPX_FWD(0, 0), { OP_BINS, 0, 0, 75 }, { OP_BINS, 1, 0, 14 }, { OP_BINS, 2, 0, 25 },
	{ OP_BINS, 3, 0, 25 }, { OP_BINS, 4, 1, 100 }, { OP_BINS, 5, 0, 10 }, SPX_BACK(1, 0), SPX_BACK(2, 1), SPX_END };
static const float HARM_CHORD[8][3] = {
	{ 1.259921f, 1.498307f, 2.f },        /* major */
	{ 1.189207f, 1.498307f, 2.f },        /* minor */
	{ 1.498307f, 2.f, 2.996614f },        /* fifths */
	{ 0.5f, 2.f, 4.f },                   /* octaves */
	{ 1.334840f, 1.498307f, 2.f },        /* sus */
	{ 1.259921f, 1.498307f, 1.781797f },  /* seventh */
	{ 1.25f, 1.5f, 1.75f },               /* otonal: 4:5:6:7 */
	{ 0.8f, 0.6666667f, 0.5714286f } };   /* utonal: 1/4:1/5:1/6:1/7 */
static uint32_t harm_lay(spx *h, float nature)
{
	int N = spx_size(nature), n2 = N / 2;
	int32_t xs[13] = { n2, n2, n2, n2, n2, n2, n2, n2, n2, n2, n2, n2, n2 };   /* 4 analysis, 3 loudest, 6 synth phases */
	return spx_lay(h, N, N / 4, SPX_HANN, 1, 3, 2, xs, 13);
}
static uint32_t mem_harmonizer(float nature) { return harm_lay(0, nature); }
static void bins_harmonizer(node *n, spx *h, int stage, int k0, int k1)
{
	if (stage < 4) { pv_bins(n, h, stage, k0, k1, 1, 2, 3); return; }
	float *L = spx_m(n, h->work[1]), *Rz = spx_m(n, h->work[2]);
	const float mix = n->s[2];
	if (stage == 5) {                             /* the dry sound, in both */
		const float *z = spx_m(n, h->work[0]), d = 1.f - mix;
		for (int k = k0; k < k1; k++) {
			if (k == 0) continue;
			L[2 * k] += d * z[2 * k]; L[2 * k + 1] += d * z[2 * k + 1];
			Rz[2 * k] += d * z[2 * k]; Rz[2 * k + 1] += d * z[2 * k + 1];
		}
		return;
	}
	static const float PAN[3][2] = { { 0.92f, 0.38f }, { 0.38f, 0.92f }, { 0.707f, 0.707f } };
	const int lg = h->lg2, par = h->frames & 1;
	int u = k0;
	while (u < k1) {
		int v = u >> lg, kk = u & (h->n2 - 1), end = ((v + 1) << lg) < k1 ? ((v + 1) << lg) : k1;
		if (kk == 0) n->s[12] = -1.f;
		float g = mix * 0.8f;
		pv_voice(n, h, kk, kk + (end - u), n->s[5 + v], 0.f, spx_m(n, h->x[7 + 2 * v + par]), spx_m(n, h->x[8 + 2 * v - par]), spx_m(n, h->x[4 + v]),
		         L, g * PAN[v][0], Rz, g * PAN[v][1], &n->s[12]);
		u = end;
	}
}
static void run_harmonizer(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) { harm_lay(h, n->nature); h->xunits = 3 * h->n2; spx_rate(h, OPS_HARM); }
	float ph = n->s[8] + (float)B * INV_SR * 0.23f;
	n->s[8] = ph - scq_floor(ph);
	if (h->busy == 0 || h->op == 0) {
		int ch = (int)scq_clamp(n->opt[0], 0.f, 7.f);
		float cents = scq_clamp(n->opt[1], 0.f, 100.f) * (1.f / 1200.f);
		float base = scq_clamp(in[1][0], -3.f, 3.f);
		static const float DET[3] = { -1.f, 1.f, 0.5f };
		for (int v = 0; v < 3; v++) {
			float wob = 0.3f * spx_sin(n->s[8] + (float)v * 0.333f);
			n->s[5 + v] = HARM_CHORD[ch][v] * scq_exp2(base + cents * (DET[v] + wob));
		}
		n->s[2] = scq_clamp(in[2][0], 0.f, 1.f);
	}
	spx_step(n, h, OPS_HARM, bins_harmonizer, in[0], 0, n->out[0], n->out[1]);
}

/* ------------------------------------------------------------------ resynth */
#define RS_MAX 40
static const spx_op OPS_RESYN[] = { SPX_FWD(0, 0), { OP_BINS, 0, 0, 8 }, { OP_BINS, 1, 0, 30 }, { OP_BINS, 2, 1, 0 }, SPX_END };
static uint32_t resyn_lay(spx *h, float nature)
{
	int N = spx_size(nature);
	int32_t xs[3] = { N / 2, 3 * RS_MAX, 8 * RS_MAX };   /* power, peaks (bin, power, taken), oscillators */
	return spx_lay(h, N, N / 4, SPX_HANN, 1, 1, 0, xs, 3);
}
static uint32_t mem_resynth(float nature) { return resyn_lay(0, nature); }
static void bins_resynth(node *n, spx *h, int stage, int k0, int k1)
{
	float *z = spx_m(n, h->work[0]), *pw = spx_m(n, h->x[0]), *lk = spx_m(n, h->x[1]), *lp = lk + RS_MAX;
	float *os = spx_m(n, h->x[2]);
	const int n2 = h->n2, K = (int)n->s[2];
	if (stage == 0) {
		for (int k = k0; k < k1; k++) pw[k] = k == 0 ? 0.f : z[2 * k] * z[2 * k] + z[2 * k + 1] * z[2 * k + 1];
	} else if (stage == 1) {                     /* the K loudest peaks, loudest first */
		if (k0 == 0) n->s[10] = 0.f;
		int cnt = (int)n->s[10];
		const float fl = 1e-7f * (float)h->N * (float)h->N;
		for (int k = k0; k < k1; k++) {
			if (k < 2 || k > n2 - 3) continue;
			float p = pw[k];
			if (!(p > fl && p > pw[k - 1] && p >= pw[k + 1])) continue;
			if (cnt >= K && p <= lp[cnt - 1]) continue;
			int j = cnt < K ? cnt++ : K - 1;
			while (j > 0 && lp[j - 1] < p) { lp[j] = lp[j - 1]; lk[j] = lk[j - 1]; j--; }
			lp[j] = p; lk[j] = (float)k;
		}
		n->s[10] = (float)cnt;
	} else {                                      /* match them to the oscillators, a peak a unit */
		float *used = lk + 2 * RS_MAX;
		if (k0 == 0) { n->s[11] = n->s[4]; for (int j = 0; j < RS_MAX; j++) used[j] = 0.f; }
		if (n->s[11] > 0.5f) return;              /* frozen: hold what sounds */
		const int cnt = (int)n->s[10];
		const float iN = 1.f / (float)h->N;
		for (int i = k0; i < k1; i++) {
			if (i == RS_MAX) {                     /* the last unit: the ramps to this frame */
				const float iH = 1.f / (float)h->H;
				for (int j = 0; j < RS_MAX; j++) {
					float *o = os + 8 * j;
					if (used[j] == 0.f) o[3] = 0.f;
					o[4] = (o[2] - o[0]) * iH; o[5] = (o[3] - o[1]) * iH;
				}
				n->s[5] = (float)h->H;
				continue;
			}
			if (i >= cnt) continue;
			int k = (int)lk[i];
			float a = 0.5f * spx_log2(pw[k - 1]), b = 0.5f * spx_log2(pw[k]), c = 0.5f * spx_log2(pw[k + 1]);
			float den = a - 2.f * b + c, d = den < -1e-9f ? scq_clamp(0.5f * (a - c) / den, -0.5f, 0.5f) : 0.f;
			float f = ((float)k + d) * iN, amp = scq_exp2(b - 0.25f * (a - c) * d) * 4.f * iN;
			int best = -1;
			float bd = 0.035f;
			for (int j = 0; j < RS_MAX; j++) {
				float *o = os + 8 * j;
				if (used[j] != 0.f || o[3] <= 0.f) continue;
				float r = scq_abs(f / o[2] - 1.f);
				if (r < bd) { bd = r; best = j; }
			}
			if (best < 0) {
				for (int j = 0; j < RS_MAX; j++) {
					float *o = os + 8 * j;
					if (used[j] == 0.f && o[3] <= 0.f && o[1] < 1e-5f) { best = j; o[0] = f; o[1] = 0.f; o[6] = 1.f; o[7] = 0.f; break; }
				}
			}
			if (best < 0) continue;
			used[best] = 1.f;
			os[8 * best + 2] = f; os[8 * best + 3] = scq_min(amp, 2.f);
		}
	}
}
static void run_resynth(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) { resyn_lay(h, n->nature); h->xunits = RS_MAX + 1; h->xcost = 25.f * RS_MAX + 60.f; spx_rate(h, OPS_RESYN); }
	n->s[2] = 1.f + scq_floor(scq_clamp(in[1][0], 0.f, 1.f) * (float)(RS_MAX - 1) + 0.5f);
	n->s[4] = scq_max(in[3][0], in[3][B - 1]) > 0.5f ? 1.f : 0.f;
	spx_step(n, h, OPS_RESYN, bins_resynth, in[0], 0, 0, 0);
	float *os = spx_m(n, h->x[2]), *y = n->out[0];
	const float ratio = scq_exp2(scq_clamp(in[2][0], -4.f, 4.f));
	int rem = (int)n->s[5];
	int ramp = rem < B ? rem : B;
	LOOP y[i] = 0.f;
	for (int j = 0; j < RS_MAX; j++) {
		float *o = os + 8 * j;
		if (o[1] <= 0.f && o[3] <= 0.f) continue;
		o[0] += o[4] * (float)ramp;
		float f = o[0] * ratio, a = o[1], da = o[5];
		if (f >= 0.48f || f <= 0.f) { o[1] = a + da * (float)ramp; continue; }
		float wr = spx_cos(f), wi = spx_sin(f), zr = o[6], zi = o[7];
		for (int i = 0; i < B; i++) {
			float t = zr * wr - zi * wi;
			zi = zr * wi + zi * wr; zr = t;
			if (i < ramp) a += da;
			y[i] += a * zi;
		}
		float g = 1.5f - 0.5f * (zr * zr + zi * zi);
		o[6] = zr * g; o[7] = zi * g; o[1] = a;
	}
	rem -= ramp;
	if (rem <= 0) {
		rem = 0;
		for (int j = 0; j < RS_MAX; j++) { float *o = os + 8 * j; o[1] = o[3]; o[0] = o[2]; o[4] = o[5] = 0.f; }
	}
	n->s[5] = (float)rem;
	LOOP y[i] = spx_ok(y[i], 4.f);
}

/* ------------------------------------------------------------------ formantshift */
static const spx_op OPS_FORM[] = { SPX_FWD(0, 0), { OP_BINS, 0, 0, 40 }, { OP_UNSPLIT, 1, 0, 45 }, { OP_REV, 1, 0, 12 },
	{ OP_IFFT, 1, 0, 30 }, { OP_BINS, 1, 0, 24 }, { OP_REV, 1, 0, 12 }, { OP_FFT, 1, 0, 30 }, { OP_SPLIT, 1, 0, 45 },
	{ OP_BINS, 2, 0, 60 }, SPX_BACK(0, 0), SPX_END };
static uint32_t form_lay(spx *h, float nature)
{
	int N = spx_size(nature);
	return spx_lay(h, N, N / 4, SPX_HANN, 1, 2, 1, 0, 0);
}
static uint32_t mem_formantshift(float nature) { return form_lay(0, nature); }
static void bins_formantshift(node *n, spx *h, int stage, int k0, int k1)
{
	float *z = spx_m(n, h->work[0]), *c = spx_m(n, h->work[1]);
	const int N = h->N, n2 = h->n2;
	if (stage == 0) {                             /* log2 |X|: a real, even spectrum */
		for (int k = k0; k < k1; k++) {
			if (k == 0) { c[0] = 0.5f * spx_log2(z[0] * z[0] + 1e-8f); c[1] = 0.5f * spx_log2(z[1] * z[1] + 1e-8f); continue; }
			c[2 * k] = 0.5f * spx_log2(z[2 * k] * z[2 * k] + z[2 * k + 1] * z[2 * k + 1] + 1e-8f);
			c[2 * k + 1] = 0.f;
		}
	} else if (stage == 1) {                      /* the cepstrum: keep the slow part (the envelope) */
		const float L = n->s[3], iL = 1.f / L, sc = 1.f / (float)n2;
		for (int k = k0; k < k1; k++)
			for (int e = 0; e < 2; e++) {
				int j = 2 * k + e, q = j < N - j ? j : N - j;
				c[j] *= (float)q < L ? (0.5f + 0.5f * spx_cos(0.5f * (float)q * iL)) * sc : 0.f;
			}
	} else {
		const float ir = n->s[2];
		for (int k = k0; k < k1; k++) {
			if (k == 0) { z[0] = z[1] = 0.f; continue; }
			float src = (float)k * ir, e;
			int i = (int)src;
			if (i >= n2 - 1) e = c[2 * (n2 - 1)];
			else e = c[2 * i] + (c[2 * i + 2] - c[2 * i]) * (src - (float)i);
			float gk = scq_exp2(scq_clamp(e - c[2 * k], -10.f, 5.f));
			z[2 * k] *= gk; z[2 * k + 1] *= gk;
		}
	}
}
static void run_formantshift(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) { form_lay(h, n->nature); spx_rate(h, OPS_FORM); }
	if (h->busy == 0 || h->op == 0) {
		n->s[2] = scq_exp2(-scq_clamp(in[1][0], -2.f, 2.f));
		n->s[3] = scq_clamp(0.0012f * SR, 6.f, (float)(h->N / 4));
	}
	spx_step(n, h, OPS_FORM, bins_formantshift, in[0], 0, n->out[0], 0);
}

/* ------------------------------------------------------------------ spectraldelay */
#define SD_N 1024
#define SD_H 512
static const spx_op OPS_SDEL[] = { SPX_FWD(0, 0), { OP_BINS, 0, 0, 60 }, { OP_BINS, 1, 2, 60 }, { OP_BINS, 2, 0, 10 }, SPX_BACK(0, 0), SPX_END };
static int sd_frames(float nature, float rate)
{
	float sec = scq_clamp(nature, 0.5f, 20.f) * 0.1f;
	return (int)(sec * rate / (float)SD_H) + 2;
}
static uint32_t sdel_lay(spx *h, float nature)
{
	int D = sd_frames(nature, (float)SCQ_RATE);
	int32_t xs[3] = { SD_N, D, D * (SD_N / 2) };
	return spx_lay(h, SD_N, SD_H, SPX_SINE, 1, 1, 1, xs, 3);
}
static uint32_t mem_spectraldelay(float nature) { return sdel_lay(0, nature); }
static void bins_spectraldelay(node *n, spx *h, int stage, int k0, int k1)
{
	float *z = spx_m(n, h->work[0]), *tmp = spx_m(n, h->x[0]), *sc = spx_m(n, h->x[1]);
	int16_t *line = (int16_t *)spx_m(n, h->x[2]);
	const int D = (int)n->s[6], n2 = h->n2, head = (int)n->s[7];
	if (stage == 0) {
		if (k0 == 0) n->s[10] = 0.f;
		const float base = n->s[2] * (float)(D - 2), sp = n->s[3], fb = n->s[4], lim = (float)SD_N / SCQ_PI * 2.f;
		const int curve = (int)n->opt[0];
		const float ilg = 1.f / (float)h->lg2;
		float mx = n->s[10];
		for (int k = k0; k < k1; k++) {
			if (k == 0) { z[0] = z[1] = tmp[0] = tmp[1] = 0.f; continue; }
			float s = curve == 2 ? spx_hash01((uint32_t)k, (uint32_t)n->s[8]) : spx_log2((float)k) * ilg;
			if (curve == 1) s = 1.f - s;
			int d = 1 + (int)(base * (1.f - sp + sp * s) + 0.5f);
			if (d > D - 1) d = D - 1;
			int slot = head - d;
			if (slot < 0) slot += D;
			const int16_t *q = line + slot * SD_N + 2 * k;
			float yr = (float)q[0] * sc[slot], yi = (float)q[1] * sc[slot];
			float vr = z[2 * k] + fb * yr, vi = z[2 * k + 1] + fb * yi;
			float m2 = vr * vr + vi * vi;
			if (m2 > 0.25f * lim * lim) {         /* the feedback saturates gently, bin by bin */
				float m = spx_sqrt(m2), g = lim * scq_tanh(m / lim) / m;
				vr *= g; vi *= g;
			}
			tmp[2 * k] = vr; tmp[2 * k + 1] = vi;
			z[2 * k] = yr; z[2 * k + 1] = yi;
			mx = scq_max(mx, scq_max(scq_abs(vr), scq_abs(vi)));
		}
		n->s[10] = mx;
	} else if (stage == 1) {
		float s = n->s[10] / 32000.f;
		n->s[11] = s > 1e-20f ? s : 1e-20f;
		sc[head] = n->s[11];
	} else {
		const float is = 1.f / n->s[11];
		int16_t *q = line + head * SD_N;
		for (int k = k0; k < k1; k++)
			for (int e = 0; e < 2; e++) {
				float v = tmp[2 * k + e] * is;
				q[2 * k + e] = (int16_t)(v >= 0.f ? v + 0.5f : v - 0.5f);
			}
		if (k1 >= n2) n->s[7] = (float)(head + 1 >= D ? 0 : head + 1);
	}
}
static void run_spectraldelay(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) {
		sdel_lay(h, n->nature); spx_rate(h, OPS_SDEL);
		int Dm = sd_frames(n->nature, (float)SCQ_RATE), D = sd_frames(n->nature, SR);
		n->s[6] = (float)(D < Dm ? D : Dm);
		n->s[7] = 0.f;
		n->s[8] = (float)(n->rng & 0xffffu);
	}
	n->opt[0] = scq_clamp(n->opt[0], 0.f, 2.f);
	if (h->busy == 0 || h->op == 0) {
		n->s[2] = scq_clamp(in[1][0], 0.f, 1.f);
		n->s[3] = scq_clamp(in[2][0], 0.f, 1.f);
		n->s[4] = scq_clamp(in[3][0], 0.f, 0.97f);
	}
	spx_step(n, h, OPS_SDEL, bins_spectraldelay, in[0], 0, n->out[0], 0);
}

/* ------------------------------------------------------------------ convolve */
/* Uniformly partitioned convolution (overlap-save, a frequency-domain delay line): partitions of
 * 512 samples, FFT 1024. The impulse response and the delay line are kept as 16-bit spectra, one
 * scale each (about -90 dB of noise), so 0.25 s fits the house. s: 0 head, 1 max, 2 mix,
 * 3 state (0 playing, 1 armed, 2 capturing, 3 building its own room), 4 samples captured,
 * 5 partition being captured, 6 energy, 7 loudness, 8 wet fade, 9 level, 10 armed for,
 * 11 last capture, 12 offset, 13 a partition is ready, 14 its max, 15 the made room's filter */
#define CV_P 512
static const spx_op OPS_CONV[] = { { OP_BINS, 7, 2, 30 }, SPX_FWD(0, 0), { OP_BINS, 0, 0, 4 }, { OP_BINS, 1, 0, 8 },
	{ OP_BINS, 2, 2, 250 }, { OP_BINS, 3, 0, 0 }, { OP_UNSPLIT, 0, 0, 45 }, { OP_REV, 0, 0, 12 }, { OP_IFFT, 0, 0, 30 },
	{ OP_SAVE, 0, 0, 8 }, { OP_BINS, 4, 0, 16 }, { OP_REV, 1, 0, 12 }, { OP_FFT, 1, 0, 30 }, { OP_SPLIT, 1, 0, 45 },
	{ OP_BINS, 5, 0, 4 }, { OP_BINS, 6, 0, 8 }, SPX_END };
static int cv_parts(float nature, float rate)
{
	float sec = scq_clamp(nature, 0.2f, 10.f) * 0.1f;
	int k = (int)(sec * rate / (float)CV_P + 0.999f);
	return k < 1 ? 1 : k;
}
static uint32_t conv_lay(spx *h, float nature)
{
	int K = cv_parts(nature, (float)SCQ_RATE);
	int32_t xs[6] = { K * CV_P, K * CV_P, K, K, K, K };
	return spx_lay(h, 2 * CV_P, CV_P, SPX_RECT, 1, 2, 1, xs, 6);
}
static uint32_t mem_convolve(float nature) { return conv_lay(0, nature); }
static inline int16_t cv_q(float v) { v = scq_clamp(v, -32767.f, 32767.f); return (int16_t)(v >= 0.f ? v + 0.5f : v - 0.5f); }
static void bins_convolve(node *n, spx *h, int stage, int k0, int k1)
{
	float *z = spx_m(n, h->work[0]), *c1 = spx_m(n, h->work[1]);
	int16_t *fdl = (int16_t *)spx_m(n, h->x[0]), *ir = (int16_t *)spx_m(n, h->x[1]);
	float *sF = spx_m(n, h->x[2]), *sH = spx_m(n, h->x[3]), *cc = spx_m(n, h->x[4]);
	int32_t *slot = (int32_t *)spx_m(n, h->x[5]);
	const int K = h->xunits, N = h->N, head = (int)n->s[0];
	switch (stage) {
	case 7: {                                     /* a new frame: the delay line moves on; a partition to capture? */
		n->s[0] = (float)(head + 1 >= K ? 0 : head + 1);
		n->s[13] = 0.f;
		if (n->s[3] == 3.f) n->s[13] = 1.f;
		else if (n->s[3] == 2.f) {
			float need = (n->s[5] + 1.f) * (float)CV_P;
			if (n->s[4] >= need) { n->s[13] = 1.f; n->s[12] = scq_min(n->s[4] - need, (float)(CV_P - 1)); }
		}
		n->s[1] = 0.f; n->s[14] = 0.f;
	} break;
	case 0: {
		float mx = n->s[1];
		for (int k = k0; k < k1; k++) mx = scq_max(mx, scq_max(scq_abs(z[2 * k]), scq_abs(z[2 * k + 1])));
		n->s[1] = mx;
	} break;
	case 1: {
		const float s = scq_max(n->s[1] / 32000.f, 1e-20f), is = 1.f / s;
		int16_t *q = fdl + head * N;
		for (int k = k0; k < k1; k++) { q[2 * k] = cv_q(z[2 * k] * is); q[2 * k + 1] = cv_q(z[2 * k + 1] * is); }
		if (k0 == 0) sF[head] = s;
	} break;
	case 2:
		for (int j = 0; j < K; j++) {
			int sl = head - j;
			if (sl < 0) sl += K;
			slot[j] = sl * N;
			cc[j] = sF[sl] * sH[j];
		}
		break;
	case 3:
		for (int k = k0; k < k1; k++) {
			float yr = 0.f, yi = 0.f;
			const int o = 2 * k;
			if (k == 0) {
				for (int j = 0; j < K; j++) {
					const int16_t *f = fdl + slot[j], *g = ir + j * N;
					yr += cc[j] * (float)f[0] * (float)g[0]; yi += cc[j] * (float)f[1] * (float)g[1];
				}
			} else {
				for (int j = 0; j < K; j++) {
					const int16_t *f = fdl + slot[j] + o, *g = ir + j * N + o;
					float fr = (float)f[0], fi = (float)f[1], gr = (float)g[0], gi = (float)g[1];
					yr += cc[j] * (fr * gr - fi * gi); yi += cc[j] * (fr * gi + fi * gr);
				}
			}
			z[o] = yr; z[o + 1] = yi;
		}
		break;
	case 4: {                                     /* the partition to capture, zero padded */
		const int ready = n->s[13] != 0.f, synth = n->s[3] == 3.f;
		const float L = (float)(K * CV_P), fade0 = 0.85f * L, ifade = 1.f / (L - fade0);
		const float dk = -1.f / (0.08f * SR);
		int st = h->fend - (int)n->s[12] - CV_P;
		while (st < 0) st += h->R;
		const float *ring = spx_m(n, h->ring[0]);
		float en = n->s[6], lp = n->s[15];
		for (int k = k0; k < k1; k++)
			for (int e = 0; e < 2; e++) {
				int j = 2 * k + e;
				float x = 0.f;
				if (ready && j < CV_P) {
					float g = n->s[5] * (float)CV_P + (float)j;
					if (synth) { lp += (scq_noise(&n->rng) - lp) * 0.4f; x = lp * scq_exp2(g * dk); }
					else { int a = st + j; if (a >= h->R) a -= h->R; x = ring[a]; }
					if (g >= fade0) x *= 0.5f + 0.5f * spx_cos(0.5f * (g - fade0) * ifade);
					en += x * x;
				}
				c1[j] = x;
			}
		n->s[6] = en; n->s[15] = lp;
	} break;
	case 5: {
		if (n->s[13] == 0.f) break;
		float mx = n->s[14];
		for (int k = k0; k < k1; k++) mx = scq_max(mx, scq_max(scq_abs(c1[2 * k]), scq_abs(c1[2 * k + 1])));
		n->s[14] = mx;
	} break;
	default: {
		if (n->s[13] == 0.f) break;
		const int kc = (int)n->s[5];
		const float s = scq_max(n->s[14] / 32000.f, 1e-20f), is = 1.f / s;
		int16_t *q = ir + kc * N;
		for (int k = k0; k < k1; k++) { q[2 * k] = cv_q(c1[2 * k] * is); q[2 * k + 1] = cv_q(c1[2 * k + 1] * is); }
		if (k1 >= h->n2) {
			sH[kc] = s;
			n->s[5] = (float)(kc + 1);
			if (kc + 1 >= K) {                    /* the room is in: as loud as what comes in */
				n->s[7] = n->s[6] > 1e-8f ? scq_min(1.f / spx_sqrt(n->s[6]), 1e4f) : 0.f;
				n->s[3] = 0.f;
			}
		}
	} break;
	}
}
static void run_convolve(node *n, const float *const in[MAX_IN])
{
	spx *h = spx_h(n);
	if (h->N == 0) {
		conv_lay(h, n->nature);
		int Km = cv_parts(n->nature, (float)SCQ_RATE), K = cv_parts(n->nature, SR);
		h->xunits = K < Km ? K : Km;
		h->xcost = 18.f * (float)h->xunits;
		spx_rate(h, OPS_CONV);
		n->s[3] = 3.f; n->s[5] = 0.f; n->s[6] = 0.f; n->s[8] = 0.f;
	}
	const float *x = in[0], *cap = in[1];
	const float ce = scq_coef_time(0.2f, SR);
	float env = n->s[9];
	int fresh = 0;
	LOOP {
		float a = scq_abs(spx_ok(x[i], 4.f));
		if (edge(cap[i], &n->s[11])) { n->s[3] = 1.f; n->s[10] = 0.f; }
		if (n->s[3] == 1.f) {
			n->s[10] += 1.f;
			if (a > scq_max(0.01f, 4.f * env) || n->s[10] > SR) {
				int pre = n->s[10] > SR ? 0 : 8;
				n->s[3] = 2.f; n->s[4] = (float)(B - i + pre); n->s[5] = 0.f; n->s[6] = 0.f;
				fresh = 1;
			}
		}
		env += (a - env) * ce;
	}
	n->s[9] = env < 1e-12f ? 0.f : env;
	if (n->s[3] == 2.f && !fresh) n->s[4] += (float)B;
	float wet[SCQ_BLOCK];
	spx_step(n, h, OPS_CONV, bins_convolve, x, 0, wet, 0);
	const float mix = scq_clamp(in[2][0], 0.f, 1.f), g = n->s[7];
	const float tgt = n->s[3] == 0.f ? 1.f : 0.f, cf = scq_coef_time(0.01f, SR);
	float fd = n->s[8];
	LOOP {
		fd += (tgt - fd) * cf;
		n->out[0][i] = spx_ok((1.f - mix) * spx_ok(x[i], 4.f) + mix * wet[i] * g * fd, 4.f);
	}
	n->s[8] = fd < 1e-6f ? 0.f : fd;
}

/* ------------------------------------------------------------------ binaural */
/* Brown & Duda's structural head: an interaural delay (Woodworth), a head-shadow shelf at each
 * ear (one pole, one zero), a pinna notch that climbs with elevation, a darker back, and
 * distance as level and air. mem: two 64-sample delay lines. s: 0-3 the position (front, right,
 * elevation, distance, smoothed), 4-7 shelf states, 8-11 notch states, 12-13 back, 14-15 air,
 * 16-17 delays, 18 write, 19 gain */
#define BN_LEN 64
static uint32_t mem_binaural(float nature) { (void)nature; return 2 * BN_LEN * sizeof(float); }
static inline float bn_hermite(const float *b, float pos)
{
	int i = (int)pos;
	float f = pos - (float)i;
	float xm = b[(i - 1) & (BN_LEN - 1)], x0 = b[i & (BN_LEN - 1)], x1 = b[(i + 1) & (BN_LEN - 1)], x2 = b[(i + 2) & (BN_LEN - 1)];
	float c1 = 0.5f * (x1 - xm), c2 = xm - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
	return ((c3 * f + c2) * f + c1) * f + x0;
}
static void run_binaural(node *n, const float *const in[MAX_IN])
{
	float *s = n->s, *bl = (float *)n->mem, *br = bl + BN_LEN;
	const float az = scq_clamp(in[1][0], -1000.f, 1000.f);
	const float tf = spx_cos(az), tr = spx_sin(az), te = scq_clamp(in[2][0], -1.f, 1.f), td = scq_clamp(in[3][0], 0.f, 1.f);
	if (s[19] == 0.f && s[0] == 0.f && s[1] == 0.f) { s[0] = tf; s[1] = tr; s[2] = te; s[3] = td; }
	const float k = scq_coef_time(0.03f, SR / (float)B);
	s[0] += (tf - s[0]) * k; s[1] += (tr - s[1]) * k; s[2] += (te - s[2]) * k; s[3] += (td - s[3]) * k;
	float vn = spx_sqrt(s[0] * s[0] + s[1] * s[1]);
	float fr = vn > 1e-4f ? s[0] / vn : 1.f, ri = vn > 1e-4f ? s[1] / vn : 0.f;
	float ce = spx_cos(s[2] * 0.25f);
	float sx = ri * ce, sy = fr * ce;               /* to the right, to the front */
	const float ac = 0.0875f / 343.f, beta = 2.f * 343.f / 0.0875f, k2 = 2.f * SR;
	float dl[2], b0[2], b1[2], a1[2];
	for (int e = 0; e < 2; e++) {
		float c = e == 0 ? -sx : sx;               /* cos of the angle from this ear's axis */
		float th = 2.f * SCQ_PI * spx_atan2(spx_sqrt(scq_max(0.f, 1.f - c * c)), c);
		float tau = th < 0.5f * SCQ_PI ? -ac * c : ac * (th - 0.5f * SCQ_PI);
		dl[e] = (ac + tau) * SR + 2.f;
		float al = 1.05f + 0.95f * spx_cos(th * (1.f / 5.2359878f));   /* 150 degrees: the shadow's deepest */
		float d = 1.f / (k2 + beta);
		b0[e] = (k2 * al + beta) * d; b1[e] = (beta - k2 * al) * d; a1[e] = (beta - k2) * d;
	}
	/* the pinna notch */
	float el = s[2], fn = 7500.f * scq_exp2(0.45f * el), gdb = -12.f * (1.f - 0.6f * scq_max(0.f, el));
	float nb0 = 1.f, nb1 = 0.f, nb2 = 0.f, na1 = 0.f, na2 = 0.f;
	if (fn < 0.42f * SR) {
		float A = scq_exp2(gdb * 0.0415241f), w = fn * INV_SR, cw = spx_cos(w), al = spx_sin(w) * (1.f / 5.f);
		float a0 = 1.f / (1.f + al / A);
		nb0 = (1.f + al * A) * a0; nb1 = -2.f * cw * a0; nb2 = (1.f - al * A) * a0; na1 = nb1; na2 = (1.f - al / A) * a0;
	}
	float back = scq_max(0.f, -sy) * 0.5f, cb = scq_coef_hz(2500.f, SR);
	float dm = 0.25f * scq_exp2(s[3] * 5.32f), gain = 1.f / scq_max(dm, 0.5f);
	float ga = scq_tan(SCQ_PI * scq_min(0.44f * SR, 20000.f * scq_exp2(-dm * 0.125f)) * INV_SR), ca = ga / (1.f + ga);
	float d0[2] = { s[16], s[17] };
	if (d0[0] == 0.f) { d0[0] = dl[0]; d0[1] = dl[1]; }
	float g0 = s[19] == 0.f ? gain : s[19];
	const float ib = 1.f / (float)B;
	int w = (int)s[18];
	float *o[2] = { n->out[0], n->out[1] }, *buf[2] = { bl, br };
	LOOP {
		float x = spx_ok(in[0][i], 4.f);
		bl[w] = x; br[w] = x;
		float t = (float)(i + 1) * ib, g = g0 + (gain - g0) * t;
		for (int e = 0; e < 2; e++) {
			float dd = d0[e] + (dl[e] - d0[e]) * t;
			float y = bn_hermite(buf[e], (float)w - dd + (float)BN_LEN);
			float *st = s + 4 + 2 * e;              /* shelf: x1, y1 */
			float sh = b0[e] * y + b1[e] * st[0] - a1[e] * st[1];
			st[0] = y; st[1] = sh;
			float *ns = s + 8 + 2 * e;              /* notch, transposed direct form II */
			float v = nb0 * sh + ns[0];
			ns[0] = nb1 * sh - na1 * v + ns[1];
			ns[1] = nb2 * sh - na2 * v;
			s[12 + e] += (v - s[12 + e]) * cb;
			v = v - back * (v - s[12 + e]);
			float av = (v - s[14 + e]) * ca, ay = av + s[14 + e];   /* air: a one-pole, zero at Nyquist */
			s[14 + e] = ay + av;
			o[e][i] = spx_ok(ay * g, 4.f);
		}
		w = (w + 1) & (BN_LEN - 1);
	}
	s[16] = dl[0]; s[17] = dl[1]; s[18] = (float)w; s[19] = gain;
	for (int j = 4; j < 16; j++) if (scq_abs(s[j]) < 1e-15f) s[j] = 0.f;
}

/* ------------------------------------------------------------------ ambi */
/* First order: the source encoded as B-format (W X Y), decoded to two channels as UHJ (Gerzon):
 * S = 0.9397 W + 0.1856 X, D = j(-0.3420 W + 0.5099 X) + 0.6555 Y, L/R = (S +- D) / 2; width scales
 * D (0 mono, 0.5 UHJ, 1 super-stereo). j is a pair of allpass chains 90 degrees apart (after Olli
 * Niemitalo). mem: the chains. s: 0-2 position (front, left, elevation cos), 3 width, 4 prev gains */
static const float AMBI_A[4] = { 0.4794000f, 0.8762184f, 0.9765975f, 0.9974992f };   /* 0.6923878^2 ... */
static const float AMBI_B[4] = { 0.1617584f, 0.7330289f, 0.9453497f, 0.9905991f };   /* 0.4021921^2 ... */
static uint32_t mem_ambi(float nature) { (void)nature; return 40 * sizeof(float); }
static inline float ambi_chain(const float *a2, float *st, float x)
{
	for (int j = 0; j < 4; j++) {                  /* y = a^2 (x + y[-2]) - x[-2] */
		float *q = st + 4 * j;
		float y = a2[j] * (x + q[3]) - q[1];
		q[1] = q[0]; q[0] = x; q[3] = q[2]; q[2] = y;
		x = y;
	}
	return x;
}
static void run_ambi(node *n, const float *const in[MAX_IN])
{
	float *s = n->s, *m = (float *)n->mem;
	const float az = scq_clamp(in[1][0], -1000.f, 1000.f);
	const float te = scq_clamp(in[2][0], -1.f, 1.f);
	const float tf = spx_cos(az), tl = -spx_sin(az), tw = scq_clamp(in[3][0], 0.f, 1.f) * 2.f;
	if (s[9] == 0.f) { s[0] = tf; s[1] = tl; s[2] = te; s[3] = tw; s[9] = 1.f; }
	const float k = scq_coef_time(0.03f, SR / (float)B);
	s[0] += (tf - s[0]) * k; s[1] += (tl - s[1]) * k; s[2] += (te - s[2]) * k; s[3] += (tw - s[3]) * k;
	float vn = spx_sqrt(s[0] * s[0] + s[1] * s[1]);
	float ce = spx_cos(s[2] * 0.25f) / (vn > 1e-4f ? vn : 1.f);
	const float Wa = 0.7071068f, X = s[0] * ce, Y = s[1] * ce;
	float gS = 0.9396926f * Wa + 0.1855740f * X, gJ = -0.3420201f * Wa + 0.5098604f * X, gT = 0.6554516f * Y;
	float gs[3] = { gS * 1.4f, gJ * 1.4f * s[3], gT * 1.4f * s[3] };
	if (s[8] == 0.f) { s[4] = gs[0]; s[5] = gs[1]; s[6] = gs[2]; s[8] = 1.f; }
	const float ib = 1.f / (float)B;
	float da = m[32];
	LOOP {
		float x = spx_ok(in[0][i], 4.f), t = (float)(i + 1) * ib;
		float a = ambi_chain(AMBI_A, m, x), b = ambi_chain(AMBI_B, m + 16, x);
		float ad = da; da = a;                      /* the first chain one sample late */
		float S = (s[4] + (gs[0] - s[4]) * t) * ad;
		float D = (s[5] + (gs[1] - s[5]) * t) * b + (s[6] + (gs[2] - s[6]) * t) * ad;
		n->out[0][i] = spx_ok(0.5f * (S + D), 4.f);
		n->out[1][i] = spx_ok(0.5f * (S - D), 4.f);
	}
	m[32] = da;
	s[4] = gs[0]; s[5] = gs[1]; s[6] = gs[2];
	for (int j = 0; j < 33; j++) if (scq_abs(m[j]) < 1e-15f) m[j] = 0.f;
}

/* ------------------------------------------------------------------ crossfeed */
/* Bauer stereophonic-to-binaural (after Boris Mikhaylov's bs2b): each ear hears the other side
 * low-passed at 700 Hz and a little late, its own side with the highs lifted to match.
 * s: 0-1 lowpass, 2-3 high shelf, 4-5 last inputs, 6 amount seen, 7-11 coefficients */
static void run_crossfeed(node *n, const float *const in[MAX_IN])
{
	float *s = n->s;
	float amt = scq_clamp(in[2][0], 0.f, 1.f);
	if (s[6] != amt + 1.f || s[12] != SR) {
		s[6] = amt + 1.f; s[12] = SR;
		float lev = 12.f - 8.f * amt;              /* dB between the sides at low frequencies */
		float gblo = lev * (-5.f / 6.f) - 3.f, gbhi = lev / 6.f - 3.f;
		float glo = scq_exp2(gblo * 0.16609640f), ghi = 1.f - scq_exp2(gbhi * 0.16609640f);
		float fhi = 700.f * scq_exp2((gblo - 6.0206f * spx_log2(ghi)) * (1.f / 12.f));
		float xl = scq_exp2(-2.f * SCQ_PI * scq_min(700.f, 0.45f * SR) * INV_SR * 1.442695f);
		float xh = scq_exp2(-2.f * SCQ_PI * scq_min(fhi, 0.45f * SR) * INV_SR * 1.442695f);
		s[7] = glo * (1.f - xl); s[8] = xl; s[9] = 1.f - ghi * (1.f - xh); s[10] = -xh; s[11] = xh;
	}
	const float a0l = s[7], b1l = s[8], a0h = s[9], a1h = s[10], b1h = s[11], dry = 1.f - scq_min(1.f, amt * 4.f);
	LOOP {
		float l = spx_ok(in[0][i], 4.f), r = spx_ok(in[1][i], 4.f);
		s[0] = a0l * l + b1l * s[0];
		s[1] = a0l * r + b1l * s[1];
		s[2] = a0h * l + a1h * s[4] + b1h * s[2];
		s[3] = a0h * r + a1h * s[5] + b1h * s[3];
		s[4] = l; s[5] = r;
		float yl = s[2] + s[1], yr = s[3] + s[0];
		n->out[0][i] = spx_ok(yl + (l - yl) * dry, 4.f);
		n->out[1][i] = spx_ok(yr + (r - yr) * dry, 4.f);
	}
	for (int j = 0; j < 4; j++) if (scq_abs(s[j]) < 1e-15f) s[j] = 0.f;
}

/* ------------------------------------------------------------------ bassman */
/* The low end of a sound system: a Linkwitz-Riley split at nature x 60 Hz, a sine an octave (or
 * two) below the fundamental of what is down there (a zero-crossing tracker and a phase-locked
 * oscillator: pure, no square-wave buzz), then drive into a peak limiter with a slow release.
 * The limiter looks 3 ms ahead (the whole sound is delayed that much), holds its peak 30 ms (longer
 * than a 33 Hz cycle) and lets go over 150 ms, so it rides the bass without bending its waves.
 * mem: 8 svf states, two 256-sample lines (the low end, the rest). s: 0 tracker lp, 1 level,
 * 2 last, 3 since, 4 period, 5 sure, 6 sub phase, 7 sub level, 8 limiter peak, 9 armed,
 * 10 lock gain, 11 last period, 12 hold, 13 gain, 14 write */
#define BM_LA 256
static uint32_t mem_bassman(float nature) { (void)nature; return (8 + 2 * BM_LA) * sizeof(float); }
static inline float bm_svf(float *ic, float a1, float a2, float a3, float x, int high)
{
	float v3 = x - ic[1], v1 = a1 * ic[0] + a2 * v3, v2 = ic[1] + a2 * ic[0] + a3 * v3;
	ic[0] = 2.f * v1 - ic[0]; ic[1] = 2.f * v2 - ic[1];
	return high ? x - 1.4142136f * v1 - v2 : v2;
}
static void run_bassman(node *n, const float *const in[MAX_IN])
{
	float *s = n->s, *ic = (float *)n->mem, *dlo = ic + 8, *dhi = dlo + BM_LA;
	const int la = (int)scq_clamp(0.003f * SR, 1.f, (float)(BM_LA - 1)), hold = (int)(0.03f * SR);
	const float cga = scq_coef_time(0.0008f, SR);
	int wp = (int)s[14];
	if (s[13] == 0.f) s[13] = 1.f;
	const float fc = scq_clamp(60.f * n->nature, 30.f, scq_min(400.f, 0.2f * SR));
	const float g = scq_tan(SCQ_PI * fc * INV_SR), a1 = 1.f / (1.f + g * (g + 1.4142136f)), a2 = g * a1, a3 = g * a2;
	const float ct = scq_coef_hz(fc * 0.7f, SR), cenv = scq_coef_time(0.05f, SR);
	const float catt = scq_coef_time(0.005f, SR), crel = scq_coef_time(0.12f, SR), clk = scq_coef_time(0.03f, SR);
	const float lrel = 1.f - scq_coef_time(0.15f, SR);
	const float sub = scq_clamp(in[1][0], 0.f, 1.f), drive = 1.f + 4.f * scq_clamp(in[2][0], 0.f, 1.f);
	const float div = n->opt[0] >= 0.5f ? 4.f : 2.f, step = 1.f / div;
	const float tmin = SR / 300.f, tmax = SR / 20.f;
	LOOP {
		float x = spx_ok(in[0][i], 4.f);
		float lo = bm_svf(ic + 2, a1, a2, a3, bm_svf(ic, a1, a2, a3, x, 0), 0);
		float hi = bm_svf(ic + 6, a1, a2, a3, bm_svf(ic + 4, a1, a2, a3, x, 1), 1);
		s[0] += (lo - s[0]) * ct;
		float t = s[0], al = scq_abs(lo);
		s[1] += (scq_abs(t) - s[1]) * cenv;
		s[3] += 1.f;
		if (t < -0.1f * s[1]) s[9] = 1.f;
		if (s[9] != 0.f && t >= 0.f && s[2] < 0.f) {   /* a rising crossing */
			float frac = t / (t - s[2]);
			float T = s[3] - frac;
			s[3] = frac; s[9] = 0.f;
			if (T > tmin && T < tmax) {
				float rel = scq_abs(T - s[11]) / T;
				s[5] += ((rel < 0.12f ? 1.f : 0.f) - s[5]) * 0.3f;
				s[4] = s[4] > 0.f ? s[4] + (T - s[4]) * 0.5f : T;
				float e = s[6] * div - scq_floor(s[6] * div + 0.5f);   /* the sub's phase, pulled to the beat */
				s[6] -= e * step * 0.25f;
			} else s[5] *= 0.7f;
			s[11] = T;
		}
		if (s[3] > tmax) s[5] *= 0.999f;
		s[2] = t;
		float lock = (s[5] > 0.5f && s[4] > 0.f) ? 1.f : 0.f;
		s[10] += (lock - s[10]) * clk;
		if (s[4] > 0.f) { s[6] += 1.f / (div * s[4]); s[6] -= scq_floor(s[6]); }
		s[7] += (al - s[7]) * (al > s[7] ? catt : crel);
		float v = (lo + sub * 1.4f * s[7] * s[10] * spx_sin(s[6])) * drive;
		float av = scq_abs(v);
		if (av >= s[8]) { s[8] = av; s[12] = (float)hold; }
		else if (s[12] > 0.f) s[12] -= 1.f;
		else s[8] *= lrel;
		float gt = s[8] > 0.8f ? 0.8f / s[8] : 1.f;
		s[13] += (gt - s[13]) * (gt < s[13] ? cga : crel);
		dlo[wp] = v; dhi[wp] = hi;
		int rp = (wp - la) & (BM_LA - 1);
		wp = (wp + 1) & (BM_LA - 1);
		float y = dlo[rp] * s[13], ay = scq_abs(y);
		if (ay > 0.9f) y = (y > 0.f ? 1.f : -1.f) * (0.9f + 0.1f * scq_tanh((ay - 0.9f) * 10.f));   /* what still gets past */
		n->out[0][i] = spx_ok(dhi[rp] + y, 4.f);
		n->out[1][i] = spx_ok(y, 4.f);
	}
	s[14] = (float)wp;
	for (int j = 0; j < 8; j++) if (scq_abs(ic[j]) < 1e-15f) ic[j] = 0.f;
	for (int j = 0; j < 12; j++) if (scq_abs(s[j]) < 1e-15f && j != 6) s[j] = 0.f;
}
