# frozen_string_literal: true

gem "aws-sdk-s3", "~> 1.14"

require "aws-sdk-s3"
require "active_support/core_ext/numeric/bytes"

module ActiveStorage
  # Wraps the Amazon Simple Storage Service (S3) as an Active Storage service.
  # See ActiveStorage::Service for the generic API documentation that applies to all services.
  class Service::S3Service < Service
    attr_reader :client, :bucket
    attr_reader :multipart_upload_threshold, :upload_options

    def initialize(bucket:, upload: {}, public: false, **options)
      @client = Aws::S3::Resource.new(**options)
      @bucket = @client.bucket(bucket)

      @multipart_upload_threshold = upload.fetch(:multipart_threshold, 100.megabytes)
      @public = public

      @upload_options = upload
      @upload_options[:acl] = "public-read" if public?
    end

    def upload(key, io, checksum: nil, filename: nil, content_type: nil, disposition: nil, **)
      instrument :upload, key: key, checksum: checksum do
        content_disposition = content_disposition_with(filename: filename, type: disposition) if disposition && filename

        if io.size < multipart_upload_threshold
          upload_with_single_part key, io, checksum: checksum, content_type: content_type, content_disposition: content_disposition
        else
          upload_with_multipart key, io, content_type: content_type, content_disposition: content_disposition
        end
      end
    end

    def download(key, &block)
      if block_given?
        instrument :streaming_download, key: key do
          stream(key, &block)
        end
      else
        instrument :download, key: key do
          object_for(key).get.body.string.force_encoding(Encoding::BINARY)
        rescue Aws::S3::Errors::NoSuchKey
          raise ActiveStorage::FileNotFoundError
        end
      end
    end

    def download_chunk(key, range)
      instrument :download_chunk, key: key, range: range do
        object_for(key).get(range: "bytes=#{range.begin}-#{range.exclude_end? ? range.end - 1 : range.end}").body.string.force_encoding(Encoding::BINARY)
      rescue Aws::S3::Errors::NoSuchKey
        raise ActiveStorage::FileNotFoundError
      end
    end

    def delete(key)
      instrument :delete, key: key do
        object_for(key).delete
      end
    end

    def delete_prefixed(prefix)
      instrument :delete_prefixed, prefix: prefix do
        bucket.objects(prefix: prefix).batch_delete!
      end
    end

    def exist?(key)
      instrument :exist, key: key do |payload|
        answer = object_for(key).exists?
        payload[:exist] = answer
        answer
      end
    end

    def url_for_direct_upload(key, expires_in:, content_type:, content_length:, checksum:)
      instrument :url, key: key do |payload|
        generated_url = object_for(key).presigned_url :put, expires_in: expires_in.to_i,
          content_type: content_type, content_length: content_length, content_md5: checksum
          whitelist_headers: ['content-length'], **upload_options

        payload[:url] = generated_url

        generated_url
      end
    end

    def headers_for_direct_upload(key, content_type:, checksum:, filename: nil, disposition: nil, **)
      content_disposition = content_disposition_with(type: disposition, filename: filename) if filename

      { "Content-Type" => content_type, "Content-MD5" => checksum, "Content-Disposition" => content_disposition }
    end

    private
      def private_url(key, expires_in:, filename:, disposition:, content_type:, **)
        object_for(key).presigned_url :get, expires_in: expires_in.to_i,
          response_content_disposition: content_disposition_with(type: disposition, filename: filename),
          response_content_type: content_type
      end

      def public_url(key, **)
        object_for(key).public_url
      end


      MAXIMUM_UPLOAD_PARTS_COUNT = 10000
      MINIMUM_UPLOAD_PART_SIZE   = 5.megabytes

      def upload_with_single_part(key, io, checksum: nil, content_type: nil, content_disposition: nil)
        object_for(key).put(body: io, content_md5: checksum, content_type: content_type, content_disposition: content_disposition, **upload_options)
      rescue Aws::S3::Errors::BadDigest
        raise ActiveStorage::IntegrityError
      end

      def upload_with_multipart(key, io, content_type: nil, content_disposition: nil)
        part_size = [ io.size.fdiv(MAXIMUM_UPLOAD_PARTS_COUNT).ceil, MINIMUM_UPLOAD_PART_SIZE ].max

        object_for(key).upload_stream(content_type: content_type, content_disposition: content_disposition, part_size: part_size, **upload_options) do |out|
          IO.copy_stream(io, out)
        end
      end


      def object_for(key)
        bucket.object(key)
      end

      # Reads the object for the given key in chunks, yielding each to the block.
      def stream(key)
        object = object_for(key)

        chunk_size = 5.megabytes
        offset = 0

        raise ActiveStorage::FileNotFoundError unless object.exists?

        while offset < object.content_length
          yield object.get(range: "bytes=#{offset}-#{offset + chunk_size - 1}").body.string.force_encoding(Encoding::BINARY)
          offset += chunk_size
        end
      end
  end
end