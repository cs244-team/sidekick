#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <fstream>



class AudioBuffer {
private:
    std::queue<std::string> buffer {};
    mutable std::mutex mutex {};
    std::condition_variable dataAvailable {};
    std::condition_variable bufferSpaceAvailable {};
    size_t maxBufferSize {};  // Maximum number of samples in the buffer

public:
    explicit AudioBuffer(size_t maxBuffer = 1024) : maxBufferSize(maxBuffer) {}

    void add_sample(const std::string& data) {
        std::unique_lock<std::mutex> lock(mutex);
        dataAvailable.wait(lock, [this]() { return buffer.size() < maxBufferSize; });
        buffer.push(data);
        lock.unlock();
        bufferSpaceAvailable.notify_one();
    }

    std::string pop() {
        std::unique_lock<std::mutex> lock(mutex);
        bufferSpaceAvailable.wait(lock, [this]() { return !buffer.empty(); });
        std::string sample = buffer.front();
        buffer.pop();
        lock.unlock();
        dataAvailable.notify_one();
        return sample;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return buffer.size();
    }

    bool is_empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return buffer.empty();
    }

    void load_samples(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open the audio file: " << filePath << std::endl;
            return;
        }

        const size_t bufferSize = 1024; // Define the size of each chunk
        // 16-bit samples
        std::vector<int16_t> buf(bufferSize);


        while (file.read(reinterpret_cast<char*>(buf.data()), buf.size() * sizeof(int16_t))) {
            size_t bytes_read = file.gcount();
            size_t samples_read = bytes_read / sizeof(int16_t);

            for(int i = 0; i < samples_read; i++){
                add_sample(std::to_string(buf[i]));
            }
        }
        file.close();
        std::cout << "Finished reading and loading the audio file." << std::endl;
    }

    static void wav_to_pcm(const std::string& filePath, const std::string& destPath){
        std::ifstream wavFile(filePath, std::ios::binary);
        std::ofstream pcmFile(destPath, std::ios::binary);

        if (!wavFile.is_open() || !pcmFile.is_open()) {
            std::cerr << "Error opening files!" << std::endl;
            return;
        }

        // WAV header sizes typically 44 bytes for PCM
        const size_t headerSize = 44;
        wavFile.seekg(headerSize);  // Skip the header

        // Read the data from the WAV file after the header
        std::vector<char> buf(1024);
        while (wavFile.read(buf.data(), buf.size())) {
            pcmFile.write(buf.data(), wavFile.gcount());
        }

        // Check for any remaining bytes in case the last read wasn't a full buffer
        if (wavFile.gcount() > 0) {
            pcmFile.write(buf.data(), wavFile.gcount());
        }

        wavFile.close();
        pcmFile.close();

        std::cout << "Conversion complete." << std::endl;
    }
};