import 'dart:io';

import 'package:bett_box/models/models.dart';
import 'package:flutter/services.dart';

const currentWebsiteChannel = MethodChannel('betterbox/current_website');

/// Matches only the current hostname, not a guessed registrable parent domain.
String currentWebsiteRule(String url) {
  final uri = Uri.tryParse(url.trim());
  if (uri == null ||
      !{'http', 'https'}.contains(uri.scheme) ||
      !uri.hasAuthority ||
      uri.host.isEmpty) {
    throw const FormatException('Only HTTP(S) pages are supported');
  }
  var host = uri.host.toLowerCase();
  if (host.startsWith('[') && host.endsWith(']')) {
    host = host.substring(1, host.length - 1);
  }
  if (host.endsWith('.')) host = host.substring(0, host.length - 1);
  final address = InternetAddress.tryParse(host);
  if (address != null) {
    final ipv6 = address.type == InternetAddressType.IPv6;
    return '${ipv6 ? 'IP-CIDR6' : 'IP-CIDR'},$host/${ipv6 ? 128 : 32},DIRECT,no-resolve';
  }
  // Reject characters that could introduce another rule field or invalid host.
  if (!RegExp(r'^[a-z0-9_-]+(\.[a-z0-9_-]+)*$').hasMatch(host)) {
    throw const FormatException('Invalid hostname');
  }
  return 'DOMAIN-SUFFIX,$host,DIRECT';
}

OverrideData prependWebsiteRule(OverrideData data, Rule rule) {
  return data.copyWith(
    enable: true,
    rule: data.rule.updateRules(
      (rules) => [rule, ...rules.where((item) => item.value != rule.value)],
    ),
  );
}
