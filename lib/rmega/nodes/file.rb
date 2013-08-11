require 'rmega/downloader'
require 'rmega/nodes/node'
require 'rmega/nodes/deletable'

module Rmega
  module Nodes
    class File < Node
      include Deletable

      def storage_url
        @storage_url ||= data['g'] || request(a: 'g', g: 1, n: handle)['g']
      end

      def size
        data['s']
      end

      def download(path)
        path = ::File.expand_path(path)
        path = Dir.exists?(path) ? ::File.join(path, name) : path

        logger.info "Download #{name} (#{size} bytes) => #{path}"

        k = decrypted_file_key
        k = [k[0] ^ k[4], k[1] ^ k[5], k[2] ^ k[6], k[3] ^ k[7]]
        nonce = decrypted_file_key[4..5]

        donwloader = Downloader.new(base_url: storage_url, filesize: size, local_path: path)

        donwloader.download do |start, buffer|
          nonce = [nonce[0], nonce[1], (start/0x1000000000) >> 0, (start/0x10) >> 0]
          Crypto::AesCtr.decrypt(k, nonce, buffer)[:data]
        end

        path
      end
    end
  end
end
