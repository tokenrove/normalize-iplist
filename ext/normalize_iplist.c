
#include <ruby.h>

static VALUE module;

      /* self.ips = ips.map { |ip| self.class.string_to_cidr(ip) }.uniq.sort.map { |ip| self.class.cidr_to_string(ip) } */
/* Normalize IP addresses, uniq and sort, in place */
static VALUE normalize_text_bang(VALUE self, VALUE in) {
    return Qnil;		/* not implemented */
}

static int compare_serialized(const void *a_, const void *b_)
{
    const uint8_t *a = a_, *b = b_;
    for (size_t i = 0; i < 5; ++i)
	if (a[i]-b[i]) return a[i]-b[i];
    return 0;
}

struct ip_and_mask {
    uint32_t ip;
    uint8_t mask;
};

struct ip_and_mask read_textual_ip_address_and_optional_mask(char *p)
{
    uint8_t a,b,c,d;
    struct ip_and_mask ip_and_mask = {0};
    int rv = sscanf(p, "%hhu.%hhu.%hhu.%hhu/%hhu", &a, &b, &c, &d, &ip_and_mask.mask);
    switch (rv) {
    case 4:
	ip_and_mask.mask = 32;
    case 5:
	ip_and_mask.ip = a<<24 | b<<16 | c<<8 | d;
	return ip_and_mask;
    default:
	ip_and_mask.mask = 0;
	return ip_and_mask;
    }
}

            /* serialized = row[:ips].map do |cidr| */
            /*   (ip, mask) = cidr.split('/') */
            /*   (ip.split('.').map(&:to_i) << (mask || 32).to_i).pack('CCCCC') */
            /* end.sort.join */
/* Serialize an array of IPs as strings into a single string of big-endian
 * integers plus mask. */
static VALUE serialize(VALUE self, VALUE in) {
    if (TYPE(in) != T_ARRAY)
	rb_raise(rb_eTypeError, "invalid type; serialize requires an array");

    size_t n = RARRAY_LEN(in);
    VALUE out = rb_str_new(NULL, n*5);
    VALUE out_str = rb_string_value(&out);
    char *out_p = RSTRING_PTR(out_str);

    for (size_t i = 0; i < n; ++i) {
	VALUE ent = rb_ary_entry(in, i);
	char *s = StringValueCStr(ent);
	struct ip_and_mask ip_and_mask = read_textual_ip_address_and_optional_mask(s);
	if (ip_and_mask.mask < 8 || ip_and_mask.mask > 32)
	    rb_raise(rb_eArgError, "invalid IP and mask");
	uint32_t ip = ip_and_mask.ip;
	*out_p++ = ip>>24; *out_p++ = ip>>16; *out_p++ = ip>>8; *out_p++ = ip;
	*out_p++ = ip_and_mask.mask;
    }
    qsort(RSTRING_PTR(out_str), n, 5, compare_serialized);
    return out;
}

void Init_normalize_iplist(void) {
    /* VALUE cNormalizeIplist = rb_const_get(rb_cObject, rb_intern("NormalizeIPList")); */
    module = rb_define_module("NormalizeIPList");
    rb_define_module_function(module, "normalize_text!", normalize_text_bang, 1);
    rb_define_module_function(module, "serialize", serialize, 1);
}
