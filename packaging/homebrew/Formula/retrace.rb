# SPDX-License-Identifier: BSD-2-Clause
#
# Homebrew formula for retrace (TODO.complete/38).
#
# Status: WORK IN PROGRESS. The version is a placeholder until
# v2.2.1 is tagged; once tagged, replace with the actual tag SHA
# via `brew bump-formula-pr`.
#
# Usage (local test):
#   brew install --build-from-source ./Formula/retrace.rb
#
# Usage (post-tap, once the tap exists):
#   brew tap riboseinc/retrace https://github.com/riboseinc/homebrew-retrace
#   brew install retrace

class Retrace < Formula
  desc "Userspace libc interceptor for security/vulnerability discovery"
  homepage "https://github.com/riboseinc/retrace"
  url "https://github.com/riboseinc/retrace/archive/refs/tags/v2.2.1.tar.gz"
  version "2.2.1"
  # sha256 "REPLACE_AFTER_TAG"
  license "BSD-2-Clause"
  head "https://github.com/riboseinc/retrace.git", branch: "main"

  depends_on :macos => :catalina_or_newer

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "openssl@3"

  def install
    args = %W[
      -DBUILD_SHARED_LIBS=ON
      -DRETRACE_BUILD_TESTS=OFF
      -DRETRACE_BUILD_EXAMPLES=OFF
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_INSTALL_PREFIX=#{prefix}
    ]

    system "cmake", "-B", "build", "-G", "Ninja", *args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  def caveats
    <<~EOS
      retrace is a preload-based interceptor. To use it:

        LD_PRELOAD=#{HOMEBREW_PREFIX}/lib/libretrace.so RETRACE_JSON_CONFIG=... <binary>

      Or on macOS:

        DYLD_INSERT_LIBRARIES=#{HOMEBREW_PREFIX}/lib/libretrace.dylib RETRACE_JSON_CONFIG=... <binary>

      macOS SIP blocks DYLD_INSERT_LIBRARIES for binaries in system
      directories -- `csrutil disable` is required to trace them.

      Documentation: #{homepage}/blob/main/docs/README.md
      Cookbook:      #{homepage}/blob/main/docs/cookbook/README.md
    EOS
  end

  test do
    output = shell_output(
      "DYLD_INSERT_LIBRARIES=#{lib}/libretrace.dylib /usr/bin/id 2>&1 || true"
    )
    assert output
  end
end
