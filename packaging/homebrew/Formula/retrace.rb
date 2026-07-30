# SPDX-License-Identifier: BSD-2-Clause
#
# Homebrew formula for retrace.
#
# Install:
#   brew tap riboseinc/retrace https://github.com/riboseinc/retrace
#   brew install retrace
#
# Or directly from this file:
#   brew install --HEAD \
#     https://raw.githubusercontent.com/riboseinc/retrace/main/packaging/homebrew/Formula/retrace.rb
#
# The formula builds from the latest release tag (or main if --HEAD).

class Retrace < Formula
  desc "Userspace libc interceptor for tracing, fuzzing, and mocking"
  homepage "https://github.com/riboseinc/retrace"
  url "https://github.com/riboseinc/retrace/archive/refs/tags/v2.1.0.tar.gz"
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  license "BSD-2-Clause"
  head "https://github.com/riboseinc/retrace.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build

  on_macos do
    depends_on "openssl@3"
  end

  on_linux do
    depends_on "openssl@3"
  end

  def install
    mkdir "build" do
      system "cmake", "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=#{prefix}",
        "-DRETRACE_BUILD_TESTS=OFF",
        "-DRETRACE_BUILD_EXAMPLES=OFF",
        ".."
      system "cmake", "--build", "."
      system "cmake", "--install", "."
    end

    # Install the cookbook and pretty-printer alongside.
    pkgshare.install "docs/cookbook"
    pkgshare.install "tools/logpp"
    bin.install_symlink pkgshare/"logpp/logpp.py" => "retrace-logpp"
  end

  def caveats
    <<~EOS
      retrace is installed.

      Quick start:
        retrace --help
        retrace list-actions
        retrace run -- #{opt_bin}/echo hello

      Smoke test (verify interception works):
        echo 'int main(void){printf("uid=%d\\n",getuid());return 0;}' \
          > /tmp/getuid.c
        cc /tmp/getuid.c -o /tmp/getuid
        retrace run \
          --config #{opt_pkgshare}/cookbook/05-mock-getuid.md \
          -- /tmp/getuid

      macOS note: SIP-protected binaries (/usr/bin/*) silently skip
      DYLD_INSERT_LIBRARIES. Copy the target to /tmp/ first.

      Pretty-printer:
        retrace run --log /tmp/trace.json -- /bin/ls
        retrace-logpp /tmp/trace.json
    EOS
  end

  test do
    assert_match "retrace v#{version}", shell_output("#{bin}/retrace --help")
  end
end
