# frozen_string_literal: true

gem "google-cloud-storage", "~> 1.11"
require "google/cloud/storage"

module ActiveStorage
  # Wraps the Google Cloud Storage as an Active Storage service. See ActiveStorage::Service for the generic API
  # documentation that applies to all services.
  class Service::GCSService < Service
    def initialize(**config)
      @config = config
    end

    def upload(key, io, checksum: nil)
      instrument :upload, key: key, checksum: checksum do
        begin
          # The official GCS client library doesn't allow us to create a file with no Content-Type metadata.
          # We need the file we create to have no Content-Type so we can control it via the response-content-type
          # param in signed URLs. Workaround: let the GCS client create the file with an inferred
          # Content-Type (usually "application/octet-stream") then clear it.
          bucket.create_file(io, key, md5: checksum).update do |file|
            file.content_type = nil
          end
        rescue Google::Cloud::InvalidArgumentError
          raise ActiveStorage::IntegrityError
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
          begin
            file_for(key).download.string
          rescue Google::Cloud::NotFoundError
            raise ActiveStorage::FileNotFoundError
          end
        end
      end
    end

    def download_chunk(key, range)
      instrument :download_chunk, key: key, range: range do
        begin
          file_for(key).download(range: range).string
        rescue Google::Cloud::NotFoundError
          raise ActiveStorage::FileNotFoundError
        end
      end
    end

    def delete(key)
      instrument :delete, key: key do
        begin
          file_for(key).delete
        rescue Google::Cloud::NotFoundError
          # Ignore files already deleted
        end
      end
    end

    def delete_prefixed(prefix)
      instrument :delete_prefixed, prefix: prefix do
        bucket.files(prefix: prefix).all do |file|
          begin
            file.delete
          rescue Google::Cloud::NotFoundError
            # Ignore concurrently-deleted files
          end
        end
      end
    end

    def exist?(key)
      instrument :exist, key: key do |payload|
        answer = file_for(key).exists?
        payload[:exist] = answer
        answer
      end
    end

    def url(key, expires_in:, filename:, content_type:, disposition:)
      instrument :url, key: key do |payload|
        generated_url = file_for(key).signed_url expires: expires_in, query: {
          "response-content-disposition" => content_disposition_with(type: disposition, filename: filename),
          "response-content-type" => content_type
        }
# frozen_string_literal: true

gem "google-cloud-storage", "~> 1.11"
require "google/cloud/storage"

module ActiveStorage
  # Wraps the Google Cloud Storage as an Active Storage service. See ActiveStorage::Service for the generic API
  # documentation that applies to all services.
  class Service::GCSService < Service
    def initialize(**config)
      @config = config
    end

    def upload(key, io, checksum: nil)
      instrument :upload, key: key, checksum: checksum do
        begin
          # The official GCS client library doesn't allow us to create a file with no Content-Type metadata.
          # We need the file we create to have no Content-Type so we can control it via the response-content-type
          # param in signed URLs. Workaround: let the GCS client create the file with an inferred
          # Content-Type (usually "application/octet-stream") then clear it.
          bucket.create_file(io, key, md5: checksum).update do |file|
            file.content_type = nil
          end
        rescue Google::Cloud::InvalidArgumentError
          raise ActiveStorage::IntegrityError
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
          begin
            file_for(key).download.string
          rescue Google::Cloud::NotFoundError
            raise ActiveStorage::FileNotFoundError
          end
        end
      end
    end

    def download_chunk(key, range)
      instrument :download_chunk, key: key, range: range do
        begin
          file_for(key).download(range: range).string
        rescue Google::Cloud::NotFoundError
          raise ActiveStorage::FileNotFoundError
        end
      end
    end

    def delete(key)
      instrument :delete, key: key do
        begin
          file_for(key).delete
        rescue Google::Cloud::NotFoundError
          # Ignore files already deleted
        end
      end
    end

    def delete_prefixed(prefix)
      instrument :delete_prefixed, prefix: prefix do
        bucket.files(prefix: prefix).all do |file|
          begin
            file.delete
          rescue Google::Cloud::NotFoundError
            # Ignore concurrently-deleted files
          end
        end
      end
    end

    def exist?(key)
      instrument :exist, key: key do |payload|
        answer = file_for(key).exists?
        payload[:exist] = answer
        answer
      end
    end

    def url(key, expires_in:, filename:, content_type:, disposition:)
      instrument :url, key: key do |payload|
        generated_url = file_for(key).signed_url expires: expires_in, query: {
          "response-content-disposition" => content_disposition_with(type: disposition, filename: filename),
          "response-content-type" => content_type
        }