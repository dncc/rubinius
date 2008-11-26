module Rubinius
  ##
  # A decode for the .rbc file format.

  class CompiledFile
    ##
    # Create a CompiledFile with +magic+ magic bytes, of version +ver+,
    # data containing a SHA1 sum of +sum+. The optional +stream+ is used
    # to lazy load the body.

    def initialize(magic, ver, sum, stream=nil)
      @magic, @version, @sum = magic, ver, sum
      @stream = stream
      @data = nil
    end

    attr_reader :magic
    attr_reader :version
    attr_reader :sum
    attr_reader :stream

    ##
    # From a stream-like object +stream+ load the data in and return a
    # CompiledFile object.

    def self.load(stream)
      magic = stream.gets.strip
      version = Integer(stream.gets.strip)
      sum = stream.gets.strip

      return new(magic, version, sum, stream)
    end

    ##
    # Writes the CompiledFile +cm+ to +file+.
    def self.dump(cm, file)
      File.open(file, "w") do |f|
        new("!RBIX", 0, "x").encode_to(f, cm)
      end
    rescue Errno::EACCES
      # just skip writing the compiled file if we don't have permissions
    end

    ##
    # Encode the contets of this CompiledFile object to +stream+ with
    # a body of +body+. Body use marshalled using CompiledFile::Marshal

    def encode_to(stream, body)
      stream.puts @magic
      stream.puts @version.to_s
      stream.puts @sum.to_s

      mar = CompiledFile::Marshal.new
      stream << mar.marshal(body)
    end

    ##
    # Return the body object by unmarshaling the data

    def body
      return @data if @data

      mar = CompiledFile::Marshal.new
      @data = mar.unmarshal(stream)
    end

    ##
    # A class used to convert an CompiledMethod to and from
    # a String.

    class Marshal

      ##
      # Read all data from +stream+ and invoke unmarshal_data

      def unmarshal(stream)
        if stream.kind_of? String
          str = stream
        else
          str = stream.read
        end

        @start = 0
        @size = str.size
        @data = str.data

        unmarshal_data
      end

      ##
      # Process a stream object +stream+ as as marshalled data and
      # return an object representation of it.

      def unmarshal_data
        kind = next_type
        case kind
        when ?t
          return true
        when ?f
          return false
        when ?n
          return nil
        when ?I
          return next_string.to_i
        when ?d
          str = next_string.chop

          # handle the special NaN, Infinity and -Infinity differently
          c = str[0]
          c = str[1] if c == ?-
          if c.between?(?0, ?9)
            return str.to_f
          else
            case str.downcase
            when "infinity"
              return 1.0 / 0.0
            when "-infinity"
              return -1.0 / 0.0
            when "nan"
              return 0.0 / 0.0
            else
              raise TypeError, "Invalid Float format: #{str}"
            end
          end
        when ?s
          count = next_string.to_i
          str = next_bytes count
          discard # remove the \n
          return str
        when ?x
          count = next_string.to_i
          str = next_bytes count
          discard # remove the \n
          return str.to_sym
        when ?S
          count = next_string.to_i
          str = next_bytes count
          discard # remove the \n
          return SendSite.new(str.to_sym)
        when ?A
          count = next_string.to_i
          obj = Array.new(count)
          i = 0
          while i < count
            obj[i] = unmarshal_data
            i += 1
          end
          return obj
        when ?p
          count = next_string.to_i
          obj = Tuple.new(count)
          i = 0
          while i < count
            obj[i] = unmarshal_data
            i += 1
          end
          return obj
        when ?i
          count = next_string.to_i
          seq = InstructionSequence.new(count)
          i = 0
          while i < count
            seq[i] = next_string.to_i
            i += 1
          end
          return seq
        when ?l
          count = next_string.to_i
          lt = LookupTable.new
          i = 0
          while i < count
            size = next_string.to_i

            key = next_bytes size
            discard # remove the \n

            val = unmarshal_data
            lt[key.to_sym] = val

            i += 1
          end

          return lt
        when ?M
          version = next_string.to_i
          if version != 1
            raise "Unknown CompiledMethod version #{version}"
          end
          cm = CompiledMethod.new
          cm.__ivars__     = unmarshal_data
          cm.primitive     = unmarshal_data
          cm.name          = unmarshal_data
          cm.iseq          = unmarshal_data
          cm.stack_size    = unmarshal_data
          cm.local_count   = unmarshal_data
          cm.required_args = unmarshal_data
          cm.total_args    = unmarshal_data
          cm.splat         = unmarshal_data
          cm.literals      = unmarshal_data
          cm.exceptions    = unmarshal_data
          cm.lines         = unmarshal_data
          cm.file          = unmarshal_data
          cm.local_names   = unmarshal_data
          return cm
        else
          raise "Unknown type '#{kind.chr}'"
        end
      end

      private :unmarshal_data

      ##
      # Returns the next character in _@data_ as a Fixnum.
      #--
      # The current format uses a one-character type indicator
      # followed by a newline. If that format changes, this
      # will break and we'll fix it.
      #++
      def next_type
        chr = @data[@start]
        @start += 2
        chr
      end

      private :next_type

      ##
      # Returns the next string in _@data_ including the trailing
      # "\n" character.
      def next_string
        count = @data.locate "\n", @start
        count = @size unless count
        str = String.from_bytearray @data, @start, count - @start
        @start = count
        str
      end

      private :next_string

      ##
      # Returns the next _count_ bytes in _@data_.
      def next_bytes(count)
        str = String.from_bytearray @data, @start, count
        @start += count
        str
      end

      private :next_bytes

      ##
      # Moves the next read pointer ahead by one character.
      def discard
        @start += 1
      end

      private :discard

      ##
      # For object +val+, return a String represetation.

      def marshal(val)
        str = ""

        case val
        when TrueClass
          str << "t\n"
        when FalseClass
          str << "f\n"
        when NilClass
          str << "n\n"
        when Fixnum, Bignum
          str << "I\n#{val}\n"
        when String
          str << "s\n#{val.size}\n#{val}\n"
        when Symbol
          s = val.to_s
          str << "x\n#{s.size}\n#{s}\n"
        when SendSite
          s = val.name.to_s
          str << "S\n#{s.size}\n#{s}\n"
        when Tuple
          str << "p\n#{val.size}\n"
          val.each do |ele|
            str << marshal(ele)
          end
        when Array
          str << "A\n#{val.size}\n"
          val.each do |ele|
            str << marshal(ele)
          end
        when Float
          str << "d\n#{val}\n"
        when InstructionSequence
          str << "i\n#{val.size}\n"
          val.opcodes.each do |op|
            str << op.to_s << "\n"
          end
        when LookupTable
          str << "l\n#{val.size}\n"
          val.each do |k,v|
            str << "#{k.to_s.size}\n#{k}\n"
            str << marshal(v)
          end
        when CompiledMethod
          str << "M\n1\n"
          str << marshal(val.__ivars__)
          str << marshal(val.primitive)
          str << marshal(val.name)
          str << marshal(val.iseq)
          str << marshal(val.stack_size)
          str << marshal(val.local_count)
          str << marshal(val.required_args)
          str << marshal(val.total_args)
          str << marshal(val.splat)
          str << marshal(val.literals)
          str << marshal(val.exceptions)
          str << marshal(val.lines)
          str << marshal(val.file)
          str << marshal(val.local_names)
        else
          raise ArgumentError, "Unknown type #{val.class}: #{val.inspect}"
        end

        return str
      end
    end
  end
end

