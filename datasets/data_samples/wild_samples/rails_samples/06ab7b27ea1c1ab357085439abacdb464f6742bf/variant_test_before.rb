        blob.variant(combine_options: {
          gravity: "center",
          resize: "100x100^",
          crop: "100x100+0+0",
        }).processed
      end
      assert_match(/racecar\.jpg/, variant.service_url)

      image = read_image(variant)
      assert_equal 100, image.width
      assert_equal 100, image.height
    ensure
      ActiveStorage.variant_processor = :mini_magick
    end
  end

  test "center-weighted crop of JPEG blob using :resize_to_fill" do
    blob = create_file_blob(filename: "racecar.jpg")
    variant = blob.variant(resize_to_fill: [100, 100]).processed
    assert_match(/racecar\.jpg/, variant.service_url)

    image = read_image(variant)
    assert_equal 100, image.width
    assert_equal 100, image.height
  end

  test "resized variation of PSD blob" do
    blob = create_file_blob(filename: "icon.psd", content_type: "image/vnd.adobe.photoshop")
    variant = blob.variant(resize: "20x20").processed
    assert_match(/icon\.png/, variant.service_url)

    image = read_image(variant)
    assert_equal "PNG", image.type
    assert_equal 20, image.width
    assert_equal 20, image.height
  end

  test "resized variation of ICO blob" do
    blob = create_file_blob(filename: "favicon.ico", content_type: "image/vnd.microsoft.icon")
    variant = blob.variant(resize: "20x20").processed
    assert_match(/icon\.png/, variant.service_url)

    image = read_image(variant)
    assert_equal "PNG", image.type
    assert_equal 20, image.width
    assert_equal 20, image.height
  end

  test "optimized variation of GIF blob" do
    blob = create_file_blob(filename: "image.gif", content_type: "image/gif")

    assert_nothing_raised do
      blob.variant(layers: "Optimize").processed
    end
  end

  test "variation of invariable blob" do
    assert_raises ActiveStorage::InvariableError do
      create_file_blob(filename: "report.pdf", content_type: "application/pdf").variant(resize: "100x100")
    end
  end

  test "service_url doesn't grow in length despite long variant options" do
    blob = create_file_blob(filename: "racecar.jpg")
    variant = blob.variant(font: "a" * 10_000).processed
    assert_operator variant.service_url.length, :<, 525
  end

  test "works for vips processor" do
    begin
      ActiveStorage.variant_processor = :vips
      blob = create_file_blob(filename: "racecar.jpg")
      variant = blob.variant(thumbnail_image: 100).processed

      image = read_image(variant)
      assert_equal 100, image.width
      assert_equal 67, image.height
    rescue LoadError
      # libvips not installed
    ensure
      ActiveStorage.variant_processor = :mini_magick
    end
  end
end