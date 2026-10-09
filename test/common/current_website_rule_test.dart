import 'package:bett_box/common/current_website_rule.dart';
import 'package:bett_box/enum/enum.dart';
import 'package:bett_box/models/models.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('current website URL', () {
    test(
      'uses the exact hostname, without path, port or parent-domain guesses',
      () {
        expect(
          currentWebsiteRule('https://News.Example.co.uk:8443/a?q=b#c'),
          'DOMAIN-SUFFIX,news.example.co.uk,DIRECT',
        );
        expect(
          currentWebsiteRule(' https://example.com./ '),
          'DOMAIN-SUFFIX,example.com,DIRECT',
        );
        expect(
          currentWebsiteRule('http://localhost/'),
          'DOMAIN-SUFFIX,localhost,DIRECT',
        );
        expect(
          currentWebsiteRule('https://xn--fiqs8s.example/'),
          'DOMAIN-SUFFIX,xn--fiqs8s.example,DIRECT',
        );
      },
    );

    test('matches only the current IP address', () {
      expect(
        currentWebsiteRule('http://127.0.0.1:8080/'),
        'IP-CIDR,127.0.0.1/32,DIRECT,no-resolve',
      );
      expect(
        currentWebsiteRule('https://[2001:db8::1]/'),
        'IP-CIDR6,2001:db8::1/128,DIRECT,no-resolve',
      );
    });

    test('rejects non-web pages and malformed hosts', () {
      for (final url in [
        '',
        'about:blank',
        'chrome://settings',
        'file:///tmp/page.html',
        'javascript:alert(1)',
        'example.com',
        'https:///page',
        'https://example.com,DIRECT/',
        'https://./',
      ]) {
        expect(
          () => currentWebsiteRule(url),
          throwsFormatException,
          reason: url,
        );
      }
    });
  });

  group('permanent rule storage', () {
    test('prepends and enables added rules, retaining the inactive list', () {
      final old = Rule.value('MATCH,DIRECT');
      final inactive = Rule.value('DOMAIN,other.example,REJECT');
      final rule = Rule.value('DOMAIN-SUFFIX,example.com,DIRECT');
      final result = prependWebsiteRule(
        OverrideData(
          rule: OverrideRule(addedRules: [old], overrideRules: [inactive]),
        ),
        rule,
      );
      expect(result.enable, isTrue);
      expect(result.rule.type, OverrideRuleType.added);
      expect(result.rule.addedRules, [rule, old]);
      expect(result.rule.overrideRules, [inactive]);
    });

    test('preserves override mode and retains unrelated rules in order', () {
      final tail = Rule.value('MATCH,DIRECT');
      final rule = Rule.value('DOMAIN-SUFFIX,example.com,DIRECT');
      final differentPolicy = Rule.value('DOMAIN-SUFFIX,example.com,REJECT');
      final result = prependWebsiteRule(
        OverrideData(
          enable: true,
          rule: OverrideRule(
            type: OverrideRuleType.override,
            overrideRules: [differentPolicy, rule, tail],
            addedRules: [tail],
          ),
        ),
        rule,
      );
      expect(result.rule.type, OverrideRuleType.override);
      expect(result.rule.overrideRules, [rule, differentPolicy, tail]);
      expect(result.rule.addedRules, [tail]);
    });
  });
}
