#include <cmath>
#include "common-pw.h"

audio_async::audio_async(int len_ms) {
    m_len_ms = len_ms;

    m_running = false;
}

audio_async::~audio_async() {
    if (m_dev_id_in) {
        pw_stream_destroy(m_stream);
        pw_main_loop_destroy(m_loop);
        pw_deinit();
    }
}

void audio_async::on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param) {
    auto obj = reinterpret_cast<audio_async*>(_data);

    /* NULL means to clear the format */
    if (param == nullptr || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(param, &obj->m_format.media_type, &obj->m_format.media_subtype) < 0)
        return;

    /* only accept raw audio */
    if (obj->m_format.media_type != SPA_MEDIA_TYPE_audio ||
            obj->m_format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    /* call a helper function to parse the format for us. */
    spa_format_audio_raw_parse(param, &obj->m_format.info.raw);

    fprintf(stdout, "capturing rate:%d channels:%d\n",
            obj->m_format.info.raw.rate, obj->m_format.info.raw.channels);
}

void audio_async::on_process(void *userdata) {
    auto obj = reinterpret_cast<audio_async*>(userdata);
    struct pw_buffer *b;
    struct spa_buffer *buf;
    float *samples, max;
    uint32_t c, n, n_channels, n_samples, peak;

    if ((b = pw_stream_dequeue_buffer(obj->m_stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = static_cast<float *>(buf->datas[0].data)) == NULL)
        return;

    n_channels = obj->m_format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / sizeof(float);

    /* move cursor up */
    if (obj->m_move)
        fprintf(stdout, "%c[%dA", 0x1b, n_channels + 1);
    fprintf(stdout, "captured %d samples\n", n_samples / n_channels);
    for (c = 0; c < obj->m_format.info.raw.channels; c++) {
        max = 0.0f;
        for (n = c; n < n_samples; n += n_channels)
            max = fmaxf(max, fabsf(samples[n]));

        peak = SPA_CLAMP(max * 30, 0, 39);

        fprintf(stdout, "channel %d: |%*s%*s| peak:%f\n",
                c, peak + 1, "*", 40 - peak, "", max);
    }
    obj->m_move = true;
    fflush(stdout);

    pw_stream_queue_buffer(obj->m_stream, b);
}

const struct pw_stream_events audio_async::stream_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .param_changed = on_stream_param_changed,
        .process = on_process,
};

bool audio_async::init(int capture_id, int sample_rate) {
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct pw_properties *props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    pw_init(nullptr, nullptr);

    /* make a main loop. If you already have another main loop, you can add
     * the fd of this pipewire mainloop to it. */
    m_loop = pw_main_loop_new(nullptr);

    pw_loop_add_signal(pw_main_loop_get_loop(m_loop), SIGINT, do_quit, this);
    pw_loop_add_signal(pw_main_loop_get_loop(m_loop), SIGTERM, do_quit, this);

    /* Create a simple stream, the simple stream manages the core and remote
     * objects for you if you don't need to deal with them.
     *
     * If you plan to autoconnect your stream, you need to provide at least
     * media, category and role properties.
     *
     * Pass your events and a user_data pointer as the last arguments. This
     * will inform you about the stream state. The most important event
     * you need to listen to is the process event where you need to produce
     * the data.
     */
    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                              PW_KEY_MEDIA_CATEGORY, "Capture",
                              PW_KEY_MEDIA_ROLE, "Music",
                              NULL);
    if (capture_id > 1)
        /* Set stream target if given on command line */
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, reinterpret_cast<const char *>(capture_id + 30));

    /* uncomment if you want to capture from the sink monitor ports */
    /* pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true"); */

    m_stream = pw_stream_new_simple(
            pw_main_loop_get_loop(m_loop),
            "audio-capture",
            props,
            &stream_events,
            this);

    /* Make one parameter with the supported formats. The SPA_PARAM_EnumFormat
     * id means that this is a format enumeration (of 1 value).
     * We leave the channels and rate empty to accept the native graph
     * rate and channels. */
    struct spa_audio_info_raw info_raw = {
            .format = SPA_AUDIO_FORMAT_F32,
            .rate = 16000,
            .channels = 6,
            .position = { 0 }
    };
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info_raw);

    /* Now connect this stream. We ask that our process function is
     * called in a realtime thread. */
    pw_stream_connect(m_stream,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      static_cast<enum pw_stream_flags>(
                              PW_STREAM_FLAG_AUTOCONNECT |
                              PW_STREAM_FLAG_MAP_BUFFERS |
                              PW_STREAM_FLAG_RT_PROCESS
                      ),
                      params, 1);

#if 0 //TODO
    // capture_spec_requested.samples  = 1024;
    capture_spec_requested.callback = [](void * userdata, uint8_t * stream, int len) {
        audio_async * audio = (audio_async *) userdata;
        audio->callback(stream, len);
    };

    if (capture_id >= 0) {
        fprintf(stderr, "%s: attempt to open capture device %d : '%s' ...\n", __func__, capture_id, SDL_GetAudioDeviceName(capture_id, SDL_TRUE));
        m_dev_id_in = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(capture_id, SDL_TRUE), SDL_TRUE, &capture_spec_requested, &capture_spec_obtained, 0);
    } else {
        fprintf(stderr, "%s: attempt to open default capture device ...\n", __func__);
        m_dev_id_in = SDL_OpenAudioDevice(nullptr, SDL_TRUE, &capture_spec_requested, &capture_spec_obtained, 0);
    }

    if (!m_dev_id_in) {
        fprintf(stderr, "%s: couldn't open an audio device for capture: %s!\n", __func__, SDL_GetError());
        m_dev_id_in = 0;

        return false;
    } else {
        fprintf(stderr, "%s: obtained spec for input device (SDL Id = %d):\n", __func__, m_dev_id_in);
        fprintf(stderr, "%s:     - sample rate:       %d\n",                   __func__, capture_spec_obtained.freq);
        fprintf(stderr, "%s:     - format:            %d (required: %d)\n",    __func__, capture_spec_obtained.format,
                capture_spec_requested.format);
        fprintf(stderr, "%s:     - channels:          %d (required: %d)\n",    __func__, capture_spec_obtained.channels,
                capture_spec_requested.channels);
        fprintf(stderr, "%s:     - samples per frame: %d\n",                   __func__, capture_spec_obtained.samples);
    }

    m_sample_rate = capture_spec_obtained.freq;

    m_audio.resize((m_sample_rate*m_len_ms)/1000);
#endif //TODO
    return true;
}

bool audio_async::resume() {
    if (!m_dev_id_in) {
        fprintf(stderr, "%s: no audio device to resume!\n", __func__);
        return false;
    }

    if (m_running) {
        fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }

    //TODO SDL_PauseAudioDevice(m_dev_id_in, 0);

    m_running = true;

    return true;
}

bool audio_async::pause() {
    if (!m_dev_id_in) {
        fprintf(stderr, "%s: no audio device to pause!\n", __func__);
        return false;
    }

    if (!m_running) {
        fprintf(stderr, "%s: already paused!\n", __func__);
        return false;
    }

    //TODO SDL_PauseAudioDevice(m_dev_id_in, 1);

    m_running = false;

    return true;
}

bool audio_async::clear() {
    if (!m_dev_id_in) {
        fprintf(stderr, "%s: no audio device to clear!\n", __func__);
        return false;
    }

    if (!m_running) {
        fprintf(stderr, "%s: not running!\n", __func__);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_audio_pos = 0;
        m_audio_len = 0;
    }

    return true;
}

// callback to be called by SDL
void audio_async::callback(uint8_t * stream, int len) {
    if (!m_running) {
        return;
    }

    const size_t n_samples = len / sizeof(float);

    m_audio_new.resize(n_samples);
    memcpy(m_audio_new.data(), stream, n_samples * sizeof(float));

    //fprintf(stderr, "%s: %zu samples, pos %zu, len %zu\n", __func__, n_samples, m_audio_pos, m_audio_len);

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_audio_pos + n_samples > m_audio.size()) {
            const size_t n0 = m_audio.size() - m_audio_pos;

            memcpy(&m_audio[m_audio_pos], stream, n0 * sizeof(float));
            memcpy(&m_audio[0], &stream[n0], (n_samples - n0) * sizeof(float));

            m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
            m_audio_len = m_audio.size();
        } else {
            memcpy(&m_audio[m_audio_pos], stream, n_samples * sizeof(float));

            m_audio_pos = (m_audio_pos + n_samples) % m_audio.size();
            m_audio_len = std::min(m_audio_len + n_samples, m_audio.size());
        }
    }
}

void audio_async::get(int ms, std::vector<float> & result) {
    if (!m_dev_id_in) {
        fprintf(stderr, "%s: no audio device to get audio from!\n", __func__);
        return;
    }

    if (!m_running) {
        fprintf(stderr, "%s: not running!\n", __func__);
        return;
    }

    result.clear();

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (ms <= 0) {
            ms = m_len_ms;
        }

        size_t n_samples = (m_sample_rate * ms) / 1000;
        if (n_samples > m_audio_len) {
            n_samples = m_audio_len;
        }

        result.resize(n_samples);

        int s0 = m_audio_pos - n_samples;
        if (s0 < 0) {
            s0 += m_audio.size();
        }

        if (s0 + n_samples > m_audio.size()) {
            const size_t n0 = m_audio.size() - s0;

            memcpy(result.data(), &m_audio[s0], n0 * sizeof(float));
            memcpy(&result[n0], &m_audio[0], (n_samples - n0) * sizeof(float));
        } else {
            memcpy(result.data(), &m_audio[s0], n_samples * sizeof(float));
        }
    }
}

void audio_async::do_quit(void *userdata, int signal_number) {
    (void) signal_number;
    auto obj = reinterpret_cast<audio_async*>(userdata);
    pw_main_loop_quit(obj->m_loop);
}
