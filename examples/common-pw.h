#pragma once

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <atomic>
#include <cstdint>
#include <vector>
#include <mutex>

//
// PipeWire Audio capture
//

class audio_async {
public:
    audio_async(int len_ms);
    ~audio_async();

    bool init(int capture_id, int sample_rate);

    // start capturing audio via the provided SDL callback
    // keep last len_ms seconds of audio in a circular buffer
    bool resume();
    bool pause();
    bool clear();

    // callback to be called by SDL
    void callback(uint8_t * stream, int len);

    // get audio data from the circular buffer
    void get(int ms, std::vector<float> & audio);

private:
    struct pw_main_loop *m_loop;
    struct pw_stream *m_stream;

    struct spa_audio_info m_format;
    unsigned m_move: 1;

    int m_dev_id_in = 0;

    int m_len_ms = 0;
    int m_sample_rate = 0;

    std::atomic_bool m_running;
    std::mutex       m_mutex;

    std::vector<float> m_audio;
    std::vector<float> m_audio_new;
    size_t             m_audio_pos = 0;
    size_t             m_audio_len = 0;

    static void do_quit(void *userdata, int signal_number);
    static const struct pw_stream_events stream_events;
    static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param);
    static void on_process(void *userdata);
};

bool pw_poll_events();
