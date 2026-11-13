/* I1346: entity decoding in the shared XML tag reader (plugins/shared/source/xml_scan.c), which every feed text
 * (podcast RSS titles, UPnP names) goes through before it reaches the catalogue. Numeric decimal and hex references,
 * named entities, CDATA, double-escaped "&amp;#039;" and malformed input must all come out as UTF-8 or stay as typed. */
#include "source/xml_scan.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check_tag(const char* xml, const char* want) {
    char out[128];

    out[0] = '\0';
    (void)sel_xml_tag_text(xml, xml + strlen(xml), "title", out, sizeof out);
    if (strcmp(out, want) != 0) {
        printf("FAIL: %s -> \"%s\", want \"%s\"\n", xml, out, want);
        failures++;
    }
}

int main(void) {
    char out[64];

    check_tag("<title>Odd Pod&#039;s Best&#8230;</title>", "Odd Pod's Best\xE2\x80\xA6");
    check_tag("<title>Odd Pod&amp;#039;s &amp;amp; more&amp;#8230;</title>", "Odd Pod's &amp; more\xE2\x80\xA6");
    check_tag("<title>A&#x2014;B &#X2019;s</title>", "A\xE2\x80\x94" "B \xE2\x80\x99s");
    check_tag("<title>&lt;b&gt; &quot;q&quot; &apos;a&apos; Fish&nbsp;&amp;&nbsp;Chips</title>",
              "<b> \"q\" 'a' Fish\xC2\xA0&\xC2\xA0" "Chips");
    check_tag("<title>&ldquo;Hi&rdquo; &lsquo;x&rsquo; a&ndash;b a&mdash;b d&hellip;</title>",
              "\xE2\x80\x9CHi\xE2\x80\x9D \xE2\x80\x98x\xE2\x80\x99 a\xE2\x80\x93" "b a\xE2\x80\x94" "b d\xE2\x80\xA6");
    check_tag("<title><![CDATA[Tom &amp; Jerry]]></title>", "Tom &amp; Jerry"); /* CDATA is raw text, not decoded */
    check_tag("<title>&#128512; smile</title>", "\xF0\x9F\x98\x80 smile");
    /* Malformed or unknown stays as typed. */
    check_tag("<title>AT&T &unknown; &#; &#xZZ; &#0; &#99999999; &amp</title>", "AT&T &unknown; &#; &#xZZ; &#0; &#99999999; &amp");

    {
        size_t used = 0;
        const char* s = "&#8217;x";

        if (sel_xml_entity(s, s + strlen(s), out, sizeof out, &used) != 3 || used != 7 || memcmp(out, "\xE2\x80\x99", 3) != 0) {
            printf("FAIL: sel_xml_entity numeric\n");
            failures++;
        }
        if (sel_xml_entity(s, s + strlen(s), out, 2, &used) != 0) {
            printf("FAIL: sel_xml_entity must refuse when the output does not fit\n");
            failures++;
        }
    }
    {
        const char* xml = "<enclosure url=\"http://a/b?x=1&amp;y=2&#38;z=3\" type=\"audio/mpeg\"/>";

        if (!sel_xml_tag_attr(xml, xml + strlen(xml), "enclosure", "url", out, sizeof out) || strcmp(out, "http://a/b?x=1&y=2&z=3") != 0) {
            printf("FAIL: attribute decode: %s\n", out);
            failures++;
        }
    }
    if (failures == 0) {
        printf("xml_scan_unit_ok\n");
    }
    return failures != 0;
}
