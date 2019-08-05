// RUN: %check_clang_tidy %s bugprone-flagged-implicit-conversions %t

// FIXME: Add something that triggers the check here.
void f() {
  /* FIXME: this is supposed to trigger the check! */
  std::string a = s;
}
// CHECK-MESSAGES: :[[@LINE-1]]:6: warning: function 'f' is insufficiently awesome [bugprone-flagged-implicit-conversions]

// FIXME: Verify the applied fix.
//   * Make the CHECK patterns specific enough and try to make verified lines
//     unique to avoid incorrect matches.
//   * Use {{}} for regular expressions.
// CHECK-FIXES: {{^}}void awesome_f();{{$}}

// FIXME: Add something that doesn't trigger the check here.
void awesome_f2();
