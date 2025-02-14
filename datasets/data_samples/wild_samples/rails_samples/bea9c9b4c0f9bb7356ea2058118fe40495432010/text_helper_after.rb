        output_buffer.respond_to?(:safe_concat) ? output_buffer.safe_concat(string) : concat(string)
      end

      # Truncates a given +text+ after a given <tt>:length</tt> if +text+ is longer than <tt>:length</tt>
      # (defaults to 30). The last characters will be replaced with the <tt>:omission</tt> (defaults to "...")
      # for a total length not exceeding <tt>:length</tt>.
      #
      # Pass a <tt>:separator</tt> to truncate +text+ at a natural break.
      #
      # Pass a block if you want to show extra content when the text is truncated.
      #
      # The result is marked as HTML-safe, but it is escaped by default, unless <tt>:escape</tt> is
      # +false+. Care should be taken if +text+ contains HTML tags or entities, because truncation
      # may produce invalid HTML (such as unbalanced or incomplete tags).
      #
      #   truncate("Once upon a time in a world far far away")
      #   # => "Once upon a time in a world..."
      #
      #   truncate("Once upon a time in a world far far away", length: 17)
      #   # => "Once upon a ti..."
      #
      #   truncate("Once upon a time in a world far far away", length: 17, separator: ' ')
      #   # => "Once upon a..."
      #
      #   truncate("And they found that many people were sleeping better.", length: 25, omission: '... (continued)')
      #   # => "And they f... (continued)"
      #
      #   truncate("<p>Once upon a time in a world far far away</p>")
      #   # => "<p>Once upon a time in a wo..."
      #
      #   truncate("Once upon a time in a world far far away") { link_to "Continue", "#" }
      #   # => "Once upon a time in a wo...<a href="#">Continue</a>"
      def truncate(text, options = {}, &block)
        if text
          length  = options.fetch(:length, 30)

          content = text.truncate(length, options)
          content = options[:escape] == false ? content.html_safe : ERB::Util.html_escape(content)
          content << capture(&block) if block_given? && text.length > length
          content
        end
      end

      # Highlights one or more +phrases+ everywhere in +text+ by inserting it into
      # a <tt>:highlighter</tt> string. The highlighter can be specialized by passing <tt>:highlighter</tt>
      # as a single-quoted string with <tt>\1</tt> where the phrase is to be inserted (defaults to
      # '<mark>\1</mark>')
      #
      #   highlight('You searched for: rails', 'rails')
      #   # => You searched for: <mark>rails</mark>
      #
      #   highlight('You searched for: ruby, rails, dhh', 'actionpack')
      #   # => You searched for: ruby, rails, dhh
      #
      #   highlight('You searched for: rails', ['for', 'rails'], highlighter: '<em>\1</em>')
      #   # => You searched <em>for</em>: <em>rails</em>
      #
      #   highlight('You searched for: rails', 'rails', highlighter: '<a href="search?q=\1">\1</a>')
      #   # => You searched for: <a href="search?q=rails">rails</a>
      def highlight(text, phrases, options = {})
        text = sanitize(text) if options.fetch(:sanitize, true)

        if text.blank? || phrases.blank?
          text
        else
          highlighter = options.fetch(:highlighter, '<mark>\1</mark>')
          match = Array(phrases).map { |p| Regexp.escape(p) }.join('|')
          text.gsub(/(#{match})(?![^<]*?>)/i, highlighter)
        end.html_safe
      end

      # Extracts an excerpt from +text+ that matches the first instance of +phrase+.
      # The <tt>:radius</tt> option expands the excerpt on each side of the first occurrence of +phrase+ by the number of characters
      # defined in <tt>:radius</tt> (which defaults to 100). If the excerpt radius overflows the beginning or end of the +text+,
      # then the <tt>:omission</tt> option (which defaults to "...") will be prepended/appended accordingly. Use the
      # <tt>:separator</tt> option to choose the delimitation. The resulting string will be stripped in any case. If the +phrase+
      # isn't found, nil is returned.
      #
      #   excerpt('This is an example', 'an', radius: 5)
      #   # => ...s is an exam...
      #
      #   excerpt('This is an example', 'is', radius: 5)
      #   # => This is a...
      #
      #   excerpt('This is an example', 'is')
      #   # => This is an example
      #
      #   excerpt('This next thing is an example', 'ex', radius: 2)
      #   # => ...next...
      #
      #   excerpt('This is also an example', 'an', radius: 8, omission: '<chop> ')
      #   # => <chop> is also an example
      #
      #   excerpt('This is a very beautiful morning', 'very', separator: ' ', radius: 1)
      #   # => ...a very beautiful...
      def excerpt(text, phrase, options = {})
        return unless text && phrase

        separator = options[:separator] || ''
        phrase    = Regexp.escape(phrase)
        regex     = /#{phrase}/i

        return unless matches = text.match(regex)
        phrase = matches[0]

        unless separator.empty?
          text.split(separator).each do |value|
            if value.match(regex)
              regex = phrase = value
              break
            end
          end
        end

        first_part, second_part = text.split(regex, 2)

        prefix, first_part   = cut_excerpt_part(:first, first_part, separator, options)
        postfix, second_part = cut_excerpt_part(:second, second_part, separator, options)

        affix = [first_part, separator, phrase, separator, second_part].join.strip
        [prefix, affix, postfix].join
      end

      # Attempts to pluralize the +singular+ word unless +count+ is 1. If
      # +plural+ is supplied, it will use that when count is > 1, otherwise
      # it will use the Inflector to determine the plural form.
      #
      #   pluralize(1, 'person')
      #   # => 1 person
      #
      #   pluralize(2, 'person')
      #   # => 2 people
      #
      #   pluralize(3, 'person', 'users')
      #   # => 3 users
      #
      #   pluralize(0, 'person')
      #   # => 0 people
      def pluralize(count, singular, plural = nil)
        word = if (count == 1 || count =~ /^1(\.0+)?$/)
          singular
        else
          plural || singular.pluralize
        end

        "#{count || 0} #{word}"
      end

      # Wraps the +text+ into lines no longer than +line_width+ width. This method
      # breaks on the first whitespace character that does not exceed +line_width+
      # (which is 80 by default).
      #
      #   word_wrap('Once upon a time')
      #   # => Once upon a time
      #
      #   word_wrap('Once upon a time, in a kingdom called Far Far Away, a king fell ill, and finding a successor to the throne turned out to be more trouble than anyone could have imagined...')
      #   # => Once upon a time, in a kingdom called Far Far Away, a king fell ill, and finding\na successor to the throne turned out to be more trouble than anyone could have\nimagined...
      #
      #   word_wrap('Once upon a time', line_width: 8)
      #   # => Once\nupon a\ntime
      #
      #   word_wrap('Once upon a time', line_width: 1)
      #   # => Once\nupon\na\ntime
      def word_wrap(text, options = {})
        line_width = options.fetch(:line_width, 80)

        text.split("\n").collect! do |line|
          line.length > line_width ? line.gsub(/(.{1,#{line_width}})(\s+|$)/, "\\1\n").strip : line
        end * "\n"
      end

      # Returns +text+ transformed into HTML using simple formatting rules.
      # Two or more consecutive newlines(<tt>\n\n</tt>) are considered as a
      # paragraph and wrapped in <tt><p></tt> tags. One newline (<tt>\n</tt>) is
      # considered as a linebreak and a <tt><br /></tt> tag is appended. This
      # method does not remove the newlines from the +text+.
      #
      # You can pass any HTML attributes into <tt>html_options</tt>. These
      # will be added to all created paragraphs.
      #
      # ==== Options
      # * <tt>:sanitize</tt> - If +false+, does not sanitize +text+.
      # * <tt>:wrapper_tag</tt> - String representing the wrapper tag, defaults to <tt>"p"</tt>
      #
      # ==== Examples
      #   my_text = "Here is some basic text...\n...with a line break."
      #
      #   simple_format(my_text)
      #   # => "<p>Here is some basic text...\n<br />...with a line break.</p>"
      #
      #   simple_format(my_text, {}, wrapper_tag: "div")
      #   # => "<div>Here is some basic text...\n<br />...with a line break.</div>"
      #
      #   more_text = "We want to put a paragraph...\n\n...right there."
      #
      #   simple_format(more_text)
      #   # => "<p>We want to put a paragraph...</p>\n\n<p>...right there.</p>"
      #
      #   simple_format("Look ma! A class!", class: 'description')
      #   # => "<p class='description'>Look ma! A class!</p>"
      #
      #   simple_format("<blink>Unblinkable.</blink>")
      #   # => "<p>Unblinkable.</p>"
      #
      #   simple_format("<blink>Blinkable!</blink> It's true.", {}, sanitize: false)
      #   # => "<p><blink>Blinkable!</blink> It's true.</p>"
      def simple_format(text, html_options = {}, options = {})
        wrapper_tag = options.fetch(:wrapper_tag, :p)

        text = sanitize(text) if options.fetch(:sanitize, true)
        paragraphs = split_paragraphs(text)

        if paragraphs.empty?
          content_tag(wrapper_tag, nil, html_options)
        else
          paragraphs.map! { |paragraph|
            content_tag(wrapper_tag, raw(paragraph), html_options, false)
          }.join("\n\n").html_safe
        end
      end

      # Creates a Cycle object whose _to_s_ method cycles through elements of an
      # array every time it is called. This can be used for example, to alternate
      # classes for table rows. You can use named cycles to allow nesting in loops.
      # Passing a Hash as the last parameter with a <tt>:name</tt> key will create a
      # named cycle. The default name for a cycle without a +:name+ key is
      # <tt>"default"</tt>. You can manually reset a cycle by calling reset_cycle
      # and passing the name of the cycle. The current cycle string can be obtained
      # anytime using the current_cycle method.
      #
      #   # Alternate CSS classes for even and odd numbers...
      #   @items = [1,2,3,4]
      #   <table>
      #   <% @items.each do |item| %>
      #     <tr class="<%= cycle("odd", "even") -%>">
      #       <td>item</td>
      #     </tr>
      #   <% end %>
      #   </table>
      #
      #
      #   # Cycle CSS classes for rows, and text colors for values within each row
      #   @items = x = [{first: 'Robert', middle: 'Daniel', last: 'James'},
      #                {first: 'Emily', middle: 'Shannon', maiden: 'Pike', last: 'Hicks'},
      #               {first: 'June', middle: 'Dae', last: 'Jones'}]
      #   <% @items.each do |item| %>
      #     <tr class="<%= cycle("odd", "even", name: "row_class") -%>">
      #       <td>
      #         <% item.values.each do |value| %>
      #           <%# Create a named cycle "colors" %>
      #           <span style="color:<%= cycle("red", "green", "blue", name: "colors") -%>">
      #             <%= value %>
      #           </span>
      #         <% end %>
      #         <% reset_cycle("colors") %>
      #       </td>
      #    </tr>
      #  <% end %>
      def cycle(first_value, *values)
        options = values.extract_options!
        name = options.fetch(:name, 'default')

        values.unshift(first_value)

        cycle = get_cycle(name)
        unless cycle && cycle.values == values
          cycle = set_cycle(name, Cycle.new(*values))
        end
        cycle.to_s
      end

      # Returns the current cycle string after a cycle has been started. Useful
      # for complex table highlighting or any other design need which requires
      # the current cycle string in more than one place.
      #
      #   # Alternate background colors
      #   @items = [1,2,3,4]
      #   <% @items.each do |item| %>
      #     <div style="background-color:<%= cycle("red","white","blue") %>">
      #       <span style="background-color:<%= current_cycle %>"><%= item %></span>
      #     </div>
      #   <% end %>
      def current_cycle(name = "default")
        cycle = get_cycle(name)
        cycle.current_value if cycle
      end

      # Resets a cycle so that it starts from the first element the next time
      # it is called. Pass in +name+ to reset a named cycle.
      #
      #   # Alternate CSS classes for even and odd numbers...
      #   @items = [[1,2,3,4], [5,6,3], [3,4,5,6,7,4]]
      #   <table>
      #   <% @items.each do |item| %>
      #     <tr class="<%= cycle("even", "odd") -%>">
      #         <% item.each do |value| %>
      #           <span style="color:<%= cycle("#333", "#666", "#999", name: "colors") -%>">
      #             <%= value %>
      #           </span>
      #         <% end %>
      #
      #         <% reset_cycle("colors") %>
      #     </tr>
      #   <% end %>
      #   </table>
      def reset_cycle(name = "default")
        cycle = get_cycle(name)
        cycle.reset if cycle
      end

      class Cycle #:nodoc:
        attr_reader :values

        def initialize(first_value, *values)
          @values = values.unshift(first_value)
          reset
        end

        def reset
          @index = 0
        end

        def current_value
          @values[previous_index].to_s
        end

        def to_s
          value = @values[@index].to_s
          @index = next_index
          return value
        end

        private

        def next_index
          step_index(1)
        end

        def previous_index
          step_index(-1)
        end

        def step_index(n)
          (@index + n) % @values.size
        end
      end

      private
        # The cycle helpers need to store the cycles in a place that is
        # guaranteed to be reset every time a page is rendered, so it
        # uses an instance variable of ActionView::Base.
        def get_cycle(name)
          @_cycles = Hash.new unless defined?(@_cycles)
          return @_cycles[name]
        end

        def set_cycle(name, cycle_object)
          @_cycles = Hash.new unless defined?(@_cycles)
          @_cycles[name] = cycle_object
        end

        def split_paragraphs(text)
          return [] if text.blank?

          text.to_str.gsub(/\r\n?/, "\n").split(/\n\n+/).map! do |t|
            t.gsub!(/([^\n]\n)(?=[^\n])/, '\1<br />') || t
          end
        end

        def cut_excerpt_part(part_position, part, separator, options)
          return "", "" unless part

          radius   = options.fetch(:radius, 100)
          omission = options.fetch(:omission, "...")

          part = part.split(separator)
          part.delete("")
          affix = part.size > radius ? omission : ""

          part = if part_position == :first
            drop_index = [part.length - radius, 0].max
            part.drop(drop_index)
          else
            part.first(radius)
          end

          return affix, part.join(separator)
        end
    end
  end
end