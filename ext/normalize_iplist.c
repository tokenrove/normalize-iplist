
#include <assert.h>
#include <stdbool.h>

#include <ruby.h>
#include <ruby/io.h>

static VALUE module;

static int compare_serialized(const void *a_, const void *b_)
{
    const uint8_t *a = a_, *b = b_;
    for (size_t i = 0; i < 5; ++i)
        if (a[i]-b[i]) return a[i]-b[i];
    return 0;
}

struct ip {
    enum { INVALID_ADDRESS = 0, SINGLE_ADDRESS, ADDRESS_RANGE } type;
    union {
        struct {
            uint32_t ip;
            uint8_t mask;
        };
        struct {
            uint32_t first, last;
        };
    };
};

static const char *read_byte(const char *p, uint8_t *val)
{
    if (!p || *p < '0' || *p > '9') return NULL;

    unsigned value = 0, digit;
    while (*p >= '0' && *p <= '9') {
        digit = (*p++) - '0';
        value = 10*value + digit;
        if (value > 255) return NULL;
    }
    *val = value;
    return p;
}

static const char *read_dot(const char *p) { return (p && *p == '.') ? p+1 : NULL; }

static const char *read_mask(const char *p, uint8_t *mask)
{
    p = read_byte(p, mask);
    return (*mask < 8 || *mask > 32) ? NULL : p;
}

static const char *read_dotted_quad(const char *p, uint32_t *out)
{
    uint8_t a, b, c, d;
    p = read_byte(p, &a); p = read_dot(p);
    p = read_byte(p, &b); p = read_dot(p);
    p = read_byte(p, &c); p = read_dot(p);
    p = read_byte(p, &d);
    if (!p) return p;
    *out = a<<24 | b<<16 | c<<8 | d;
    return p;
}

static struct ip read_textual_ip_line(const char *p)
{
    struct ip ip = {0};
    uint32_t dq = 0;

    p = read_dotted_quad(p, &dq);
    if (!p) return ip;
    switch (*p++) {
    case ',':
        ip.first = dq;
        p = read_dotted_quad(p, &ip.last);
        if (NULL == p || *p) return ip;
        if (ip.last < ip.first) return ip;
        ip.type = ADDRESS_RANGE;
        break;
    case '/':
        ip.ip = dq;
        p = read_mask(p, &ip.mask);
        if (NULL == p || *p) return ip;
        ip.type = SINGLE_ADDRESS;
        break;
    case 0:
        ip.ip = dq;
        ip.mask = 32;
        ip.type = SINGLE_ADDRESS;
        break;
    default:
        /* This is unnecessary, but makes our intent explicit. */
        ip.type = INVALID_ADDRESS;
    }
    return ip;
}


static char *dedup(char *p, size_t n)
{
    char *q = p;

    for (size_t i = 1; i < n; ++i) {
        p += 5;
        if (!memcmp(p, q, 5))
            continue;
        q += 5;
        if (q < p)
            memcpy(q, p, 5);
    }
    return q + (n ? 5 : 0);
}


/* Serialize an array of IPs as strings into a single string of big-endian
 * integers plus mask. */
static VALUE serialize(VALUE self __attribute__((unused)), VALUE in) {
    if (TYPE(in) != T_ARRAY)
        rb_raise(rb_eTypeError, "serialize requires an array");

    size_t n = RARRAY_LEN(in), total = n, trim = 0;
    VALUE out = rb_str_new(NULL, n*5);
    char *out_p = RSTRING_PTR(out);
    VALUE extras = rb_ary_new2(0);

#define EMIT(p, ip, mask) do {                                          \
        *(p)++ = (ip)>>24; *(p)++ = (ip)>>16; *(p)++ = (ip)>>8; *(p)++ = (ip); \
        *(p)++ = (mask);                                                \
    } while(0)

    for (size_t i = 0; i < n; ++i) {
        VALUE ent = rb_ary_entry(in, i);
        struct ip ip = read_textual_ip_line(StringValueCStr(ent));
        switch (ip.type) {
        case SINGLE_ADDRESS:
            ip.ip &= (uint32_t)0xffffffff << (32-ip.mask);
            EMIT(out_p, ip.ip, ip.mask);
            break;
        case ADDRESS_RANGE:
            ++trim;
            total += ip.last-ip.first; /* NB: one less because we've been counted once */
            VALUE tmp = rb_str_new(NULL, (1+ip.last-ip.first)*5);
            char *p = RSTRING_PTR(tmp);
            for (uint32_t v = ip.first; v <= ip.last; ++v)
                EMIT(p, v, 32);
            rb_ary_push(extras, tmp);
            break;
        case INVALID_ADDRESS:
        default:
            rb_raise(rb_eArgError, "invalid address");
        }
    }

#undef EMIT

    if (trim != 0) {
        rb_str_set_len(out, (n-trim)*5);
        rb_str_concat(out, rb_ary_join(extras, rb_str_new(NULL, 0)));
    }
    char *outp = RSTRING_PTR(out);
    qsort(outp, total, 5, compare_serialized);
    char *trimmedp = dedup(outp, total);
    rb_str_set_len(out, trimmedp-outp);
    return out;
}

enum { MAX_IP_FMT_LEN = 4*4+4 };

static void format_ip(uint8_t *in, char *out)
{
    if (in[4] == 32)
        snprintf(out, MAX_IP_FMT_LEN, "%hhu.%hhu.%hhu.%hhu", in[0], in[1], in[2], in[3]);
    else
        snprintf(out, MAX_IP_FMT_LEN, "%hhu.%hhu.%hhu.%hhu/%hhu", in[0], in[1], in[2], in[3], in[4]);
}

static uint8_t *coalesce_networks(uint8_t *p, uint8_t *e)
{
    if (32 != p[4])
        return p;

    uint8_t *q;
    uint32_t start = p[0]<<24 | p[1]<<16 | p[2]<<8 | p[3];

    size_t best = 0;
    for (size_t n = 3; n < 32; best = n, ++n) {
        if ((start & ((1<<n)-1)) != 0) break;
        q = &p[5<<n];
        if (q > e) break;
        q -= 5;
        if (32 != q[4]) break;
        uint32_t this = q[0]<<24 | q[1]<<16 | q[2]<<8 | q[3];
        if ((this-start) != (1u<<n)-1) break;
    }

    if (best == 0)
        return p;

    q = &p[5<<best];
    q -= 5;
    uint32_t this = q[0]<<24 | q[1]<<16 | q[2]<<8 | q[3];
    this &= ~((1<<best)-1);
    q[4] = 32-best;
    q[0] = this>>24; q[1] = this>>16; q[2] = this>>8; q[3] = this;
    return q;
}

/* self.ips = ips.map { |ip| self.class.string_to_cidr(ip) }.uniq.sort.map { |ip| self.class.cidr_to_string(ip) } */
/* Normalize IP addresses, uniq and sort, return array. */
static VALUE normalize_text(VALUE self, VALUE in)
{
    VALUE serialized = serialize(self, in);
    VALUE sorted_bin = rb_string_value(&serialized);
    uint8_t *p = (uint8_t *)RSTRING_PTR(sorted_bin), *e = p + RSTRING_LEN(sorted_bin), *q = NULL;
    VALUE out = rb_ary_new();
    if (e-p < 5) return out;

    char buf[MAX_IP_FMT_LEN+1] = {0};
    for (; p+5 <= e; p += 5) {
        p = coalesce_networks(p, e);
        if (q && p[0] == q[0] && p[1] == q[1] && p[2] == q[2] && p[3] == q[3] && p[4] == q[4])
            continue;
        format_ip(p, buf);
        rb_ary_push(out, rb_str_new2(buf));
        q = p;
    }
    return out;
}

/* Returns an array of up to n (default 1) invalid IPs in the array of strings passed in. */
static VALUE validate_array(VALUE in, VALUE out, long n) {
    long len = RARRAY_LEN(in);
    if (len >= FIXNUM_MAX)
        rb_raise(rb_eArgError, "array is too large (must have fixnum indices)");

    for (long i = 0; i < len; ++i) {
        VALUE ent = rb_ary_entry(in, i);
        struct ip ip = read_textual_ip_line(StringValueCStr(ent));
        if (INVALID_ADDRESS == ip.type) {
            rb_ary_push(out, INT2FIX(1+i));
            if (RARRAY_LEN(out) >= n)
                return out;
        }
    }
    return out;
}

static VALUE validate_generic_stream(VALUE in, VALUE out, long n) {
    ID read_sym = rb_intern("read");
    /* Note about this size:
     * - it should be tuned;
     * - it should be at least as large as StringMax, the cut-off
     *   point for StringIO/file in open-uri (10240 in 2.2.0).
     *
     * It's useful to change this to a value like 3 when testing to
     * make sure the continuation logic works.
     */
    VALUE bufsize = INT2FIX(65536);
    VALUE buf = rb_str_new(NULL, bufsize);

    long line_number = 1;
    enum { INVALID,
           START,
           AFTER_COMMA,
           SEEKING_DIGIT,
           SEEKING_DOT,
           SEEKING_OCTET_DOT,
           SEEKING_MASK_DIGIT,
           SEEKING_MASKLESS_TERMINAL,
           SEEKING_MASK_TERMINAL,
           SEEKING_NEWLINE
    } state = START;

    int current_octet = 0, n_octets = 1;
    bool comma_valid = true;
    bool has_more = Qnil != rb_funcall(in, read_sym, 2, bufsize, buf);
    uint8_t *p = (uint8_t *)RSTRING_PTR(buf),
        *e = p + RSTRING_LEN(buf);
    while (has_more) {
        char c = *p;

        switch (state) {
        case START:
            comma_valid = true;
        case AFTER_COMMA:
            current_octet = n_octets = 0;
        case SEEKING_DIGIT:
            ++n_octets;
            if (c >= '0' && c <= '9') {
                current_octet = c-'0';
                state = (n_octets == 4) ? SEEKING_MASKLESS_TERMINAL : SEEKING_OCTET_DOT;
            } else
                state = INVALID;
            break;

        case SEEKING_DOT:
            if (c == '.') {
                state = (n_octets >= 4) ? INVALID : SEEKING_DIGIT;
            } else
                state = INVALID;
            break;

        case SEEKING_OCTET_DOT:
            if (c >= '0' && c <= '9') {
                current_octet = 10*current_octet + (c-'0');
                if (current_octet > 255)
                    state = INVALID;
            } else if (c == '.') {
                state = (n_octets >= 4) ? INVALID : SEEKING_DIGIT;
            } else
                state = INVALID;
            break;

        case SEEKING_MASKLESS_TERMINAL:
            if (c >= '0' && c <= '9') {
                current_octet = 10*current_octet + (c-'0');
                if (current_octet > 255)
                    state = INVALID;
            } else if (c == '/') {
                state = SEEKING_MASK_DIGIT;
            } else if (c == ',' && comma_valid) {
                comma_valid = false;
                state = AFTER_COMMA;
            } else if (c == '\r') {
                state = SEEKING_NEWLINE;
            } else if (c == '\n') {
                ++line_number;
                state = START;
            } else
                state = INVALID;
            break;

        case SEEKING_MASK_DIGIT:
            if (c >= '1' && c <= '9') {
                current_octet = c-'0';
                state = SEEKING_MASK_TERMINAL;
            } else
                state = INVALID;
            break;

        case SEEKING_MASK_TERMINAL:
            if (c >= '0' && c <= '9') {
                current_octet = 10*current_octet + (c-'0');
                if (current_octet > 32)
                    state = INVALID;
            } else if (c == ',' && comma_valid) {
                comma_valid = false;
                state = AFTER_COMMA;
            } else if (c == '\n') {
                ++line_number;
                state = START;
            } else if (c == '\r') {
                state = SEEKING_NEWLINE;
            } else
                state = INVALID;
            break;

        case SEEKING_NEWLINE:
            if (c == '\n') {
                ++line_number;
                state = START;
            } else
                state = INVALID;
            break;

        default:
            break;
        }

        ++p;
        if (p >= e) {
            has_more = Qnil != rb_funcall(in, read_sym, 2, bufsize, buf);
            if (!has_more) c = '\n';
            p = has_more ? (uint8_t *)RSTRING_PTR(buf) : NULL;
            e = has_more ? p + RSTRING_LEN(buf) : NULL;
        }

        if ('\n' == c &&
            !(START == state ||
              SEEKING_NEWLINE == state ||
              SEEKING_MASKLESS_TERMINAL == state ||
              SEEKING_MASK_TERMINAL == state)) {
            if (line_number > FIXNUM_MAX)
                rb_raise(rb_eArgError, "input was too large (more than fixnum lines)");
            rb_ary_push(out, INT2FIX(line_number));
            if (RARRAY_LEN(out) >= n)
                return out;

            ++line_number;
            state = START;
        }
    }

    return out;
}

static VALUE validate(int argc, VALUE *argv, VALUE self __attribute__((unused)))
{
    if (argc > 2 || argc == 0)
        rb_raise(rb_eArgError, "wrong number of arguments");

    VALUE in = argv[0];

    long n = 1;
    if (argc == 2) {
        if (TYPE(argv[1]) != T_FIXNUM)
            rb_raise(rb_eTypeError, "n should be a fixnum");
        n = NUM2INT(argv[1]);
        if (n <= 0)
            rb_raise(rb_eArgError, "n must be strictly positive");
    }

    VALUE out = rb_ary_new();

    if (TYPE(in) == T_ARRAY)
        return validate_array(in, out, n);
    /* XXX if this is an fd-backed IO, take a fast path here */
    return validate_generic_stream(in, out, n);
}


static VALUE strip_invalid_lines(VALUE self __attribute__((unused)), VALUE source, VALUE sink)
{
    ID read_sym = rb_intern("read");
    ID write_sym = rb_intern("write");

    /* Note about this size:
     * - it should be tuned;
     * - it should be at least as large as StringMax, the cut-off
     *   point for StringIO/file in open-uri (10240 in 2.2.0).
     *
     * It's useful to change this to a value like 3 when testing to
     * make sure the continuation logic works.
     */
    VALUE bufsize = INT2FIX(65536);
    VALUE source_buf = rb_str_new(NULL, bufsize);
    VALUE sink_buf = rb_str_new(NULL, bufsize);
    size_t line_buf_len = 34;
    uint8_t line_buf[line_buf_len+1];

    enum { INVALID,
           START,
           AFTER_COMMA,
           SEEKING_DIGIT,
           SEEKING_DOT,
           SEEKING_OCTET_DOT,
           SEEKING_MASK_DIGIT,
           SEEKING_MASKLESS_TERMINAL,
           SEEKING_MASK_TERMINAL,
           SEEKING_NEWLINE
    } state = START;

    int current_octet = 0, n_octets = 1;
    bool comma_valid = true;
    bool has_more = Qnil != rb_funcall(source, read_sym, 2, bufsize, source_buf);
    uint8_t *ip = (uint8_t *)RSTRING_PTR(source_buf),
        *ie = ip + RSTRING_LEN(source_buf),
        *op = (uint8_t *)RSTRING_PTR(sink_buf),
        *oe = op + bufsize,
        *lp = line_buf;
    while (has_more) {
        char c = *ip;
        *lp++ = c;
        if (lp-line_buf >= (ptrdiff_t)line_buf_len) {
            lp = line_buf;
            state = INVALID;
        }

#define COPY_TO_SINK() do {                     \
            assert(lp > line_buf);              \
            size_t len = lp-line_buf;           \
            line_buf[line_buf_len] = 0;         \
            memcpy(op, line_buf, len);          \
            op += len;                          \
            assert(op <= oe);                   \
            lp = line_buf;                      \
        } while (0)

        assert(oe-op >= (ptrdiff_t)line_buf_len);

        switch (state) {
        case START:
            comma_valid = true;
        case AFTER_COMMA:
            current_octet = n_octets = 0;
        case SEEKING_DIGIT:
            ++n_octets;
            if (c >= '0' && c <= '9') {
                current_octet = c-'0';
                state = (n_octets == 4) ? SEEKING_MASKLESS_TERMINAL : SEEKING_OCTET_DOT;
            } else
                state = INVALID;
            break;

        case SEEKING_DOT:
            if (c == '.') {
                state = (n_octets >= 4) ? INVALID : SEEKING_DIGIT;
            } else
                state = INVALID;
            break;

        case SEEKING_OCTET_DOT:
            if (c >= '0' && c <= '9') {
                current_octet = 10*current_octet + (c-'0');
                if (current_octet > 255)
                    state = INVALID;
            } else if (c == '.') {
                state = (n_octets >= 4) ? INVALID : SEEKING_DIGIT;
            } else
                state = INVALID;
            break;

        case SEEKING_MASKLESS_TERMINAL:
            if (c >= '0' && c <= '9') {
                current_octet = 10*current_octet + (c-'0');
                if (current_octet > 255)
                    state = INVALID;
            } else if (c == '/') {
                state = SEEKING_MASK_DIGIT;
            } else if (c == ',' && comma_valid) {
                comma_valid = false;
                state = AFTER_COMMA;
            } else if (c == '\r') {
                state = SEEKING_NEWLINE;
            } else if (c == '\n') {
                COPY_TO_SINK();
                state = START;
            } else
                state = INVALID;
            break;

        case SEEKING_MASK_DIGIT:
            if (c >= '1' && c <= '9') {
                current_octet = c-'0';
                state = SEEKING_MASK_TERMINAL;
            } else
                state = INVALID;
            break;

        case SEEKING_MASK_TERMINAL:
            if (c >= '0' && c <= '9') {
                current_octet = 10*current_octet + (c-'0');
                if (current_octet > 32)
                    state = INVALID;
            } else if (c == ',' && comma_valid) {
                comma_valid = false;
                state = AFTER_COMMA;
            } else if (c == '\n') {
                COPY_TO_SINK();
                state = START;
            } else if (c == '\r') {
                state = SEEKING_NEWLINE;
            } else
                state = INVALID;
            break;

        case SEEKING_NEWLINE:
            if (c == '\n') {
                COPY_TO_SINK();
                state = START;
            } else
                state = INVALID;
            break;

        default:
            break;
        }

        ++ip;
        if (ip >= ie) {
            has_more = Qnil != rb_funcall(source, read_sym, 2, bufsize, source_buf);
            if (!has_more) c = '\n';
            ip = has_more ? (uint8_t *)RSTRING_PTR(source_buf) : NULL;
            ie = has_more ? ip + RSTRING_LEN(source_buf) : NULL;
        }

        if ('\n' == c &&
            !(START == state ||
              SEEKING_NEWLINE == state ||
              SEEKING_MASKLESS_TERMINAL == state ||
              SEEKING_MASK_TERMINAL == state)) {
            lp = line_buf;
            state = START;
        }

        if (op + line_buf_len >= oe) {
            uint8_t *tp = (uint8_t *)RSTRING_PTR(sink_buf);
            if (op-tp < (ptrdiff_t)bufsize) {
                VALUE s = rb_str_substr(sink_buf, 0, op-tp);
                rb_funcall(sink, write_sym, 1, s);
            } else
                rb_funcall(sink, write_sym, 1, sink_buf);
            op = (uint8_t *)RSTRING_PTR(sink_buf);
        }
    }

    if (lp > line_buf &&
        (START == state ||
         SEEKING_NEWLINE == state ||
         SEEKING_MASKLESS_TERMINAL == state ||
         SEEKING_MASK_TERMINAL == state)) {
        COPY_TO_SINK();
    }

    {
        uint8_t *tp = (uint8_t *)RSTRING_PTR(sink_buf);
        if (op-tp > 0) {
            VALUE s = rb_str_substr(sink_buf, 0, op-tp);
            rb_funcall(sink, write_sym, 1, s);
        }
    }

    return Qtrue;
}


void Init_normalize_iplist(void) {
    module = rb_define_module("NormalizeIPList");
    rb_define_module_function(module, "normalize_text", normalize_text, 1);
    rb_define_module_function(module, "serialize", serialize, 1);
    rb_define_module_function(module, "validate", validate, -1);
    rb_define_module_function(module, "strip_invalid_lines", strip_invalid_lines, 2);
}
