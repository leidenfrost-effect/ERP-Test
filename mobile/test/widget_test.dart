import 'package:flutter_test/flutter_test.dart';

import 'package:hybrid_mobile/main.dart';

void main() {
  testWidgets('renders app shell', (WidgetTester tester) async {
    await tester.pumpWidget(const HybridMobileApp());
    expect(find.text('Hybrid Person Mobile'), findsOneWidget);
  });
}
