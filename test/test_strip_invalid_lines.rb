require File.dirname(__FILE__) + "/helper.rb"

class NormalizeIPListStripInvalidLines < Test::Unit::TestCase
  TYPICAL_BAD_LIST = ['cephalalgia',
                      '192.168.0.1/32',
                      '192.168.0.',
                      '192.168._',
                      'foo',
                      '127.000.000.001,127.255.255.255',
                      '192.168.0.',
                      'f192.168.0.1',
                      '10.0.0.1'].freeze
  TYPICAL_BAD_LIST_WHEAT = ['192.168.0.1/32',
                            '127.000.000.001,127.255.255.255',
                            '10.0.0.1'].freeze
  LF = "\n".freeze
  CRLF = "\r\n".freeze

  def test_strip_invalid_lines_stringio
    [LF, CRLF].each do |sep|
      source = StringIO.new(TYPICAL_BAD_LIST.join(sep))
      sink = StringIO.new
      valid_count = NormalizeIPList.strip_invalid_lines(source, sink)
      sink.rewind
      assert_equal(TYPICAL_BAD_LIST_WHEAT.join(sep), sink.read)
      assert_equal(TYPICAL_BAD_LIST_WHEAT.length, valid_count)
    end
  end

  def test_strip_invalid_files_fd
    [LF, CRLF].each do |sep|
      Tempfile.open('ip-list-source') do |source|
        Tempfile.open('ip-list-sink') do |sink|
          source.write(TYPICAL_BAD_LIST.join(sep) + sep)
          source.rewind
          valid_count = NormalizeIPList.strip_invalid_lines(source, sink)
          sink.rewind
          assert_equal(TYPICAL_BAD_LIST_WHEAT.join(sep) + sep, sink.read)
          assert_equal(TYPICAL_BAD_LIST_WHEAT.length, valid_count)
        end
      end
    end
  end

  def test_strip_from_fd_with_more_entries
    Tempfile.open('ip-list-source') do |source|
      Tempfile.open('ip-list-sink') do |sink|
        source.write(['192.168.0.1/32',
                      '192.168.0.',
                      '192.168._',
                      'foo',
                      '192.168.0.',
                      '192.168.1.42/8',
                      '192.168.1.42/0',
                      '192.168.1.42/33',
                      '192.168.1.42/3.',
                      '....',
                      '42.42.42.42/.10',
                      '255..255.255',
                      '255.255.255.255/',
                      '255.255.255.255/.',
                      '255.255.255./8',
                      '255.0255.255.0',
                      'f192.168.0.1'].join("\n"))
        source.rewind
        valid_count = NormalizeIPList.strip_invalid_lines(source, sink)
        sink.rewind
        assert_equal(['192.168.0.1/32',
                      '192.168.1.42/8',
                      '255.0255.255.0'].join("\n") + "\n",
                     sink.read)
        assert_equal(3, valid_count)
      end
    end
  end

  def test_strip_long_lines_stringio
    source = StringIO.new((['9' * 2048] * 1024).join("\n"))
    sink = StringIO.new
    valid_count = NormalizeIPList.strip_invalid_lines(source, sink)
    sink.rewind
    assert_equal('', sink.read)
    assert_equal(0, valid_count)
  end

  def test_strip_empty_stringio
    source = StringIO.new
    sink = StringIO.new
    valid_count = NormalizeIPList.strip_invalid_lines(source, sink)
    sink.rewind
    assert_equal('', sink.read)
    assert_equal(0, valid_count)
  end
end
