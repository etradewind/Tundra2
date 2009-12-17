#include "StableHeaders.h"

#include "FarsightChannel.h"
#include <TelepathyQt4/Farsight/Channel>


namespace TelepathyIM
{
    FarsightChannel::FarsightChannel(const Tp::StreamedMediaChannelPtr &channel, 
                                     const QString &audio_src_name, 
                                     const QString &audio_sink_name, 
                                     const QString &video_src_name) 
                                     : QObject(0),
                                     tp_channel_(channel),
                                     tf_channel_(0),
                                     bus_(0),
                                     pipeline_(0),
                                     audio_input_(0), 
                                     //audio_output_(0),
                                     video_input_(0),
                                     video_tee_(0), 
                                     audio_volume_(0),
                                     audio_resample_(0),
                                     audio_in_src_pad_(0),
                                     video_in_src_pad_(0),
                                     audio_stream_in_clock_rate_(0),
                                     audio_capsfilter_(0),
                                     audio_convert_(0),
                                     video_preview_widget_(0),
                                     video_remote_output_widget_(0)
    {
        CreateTfChannel();
        CreatePipeline();
        CreateAudioInputElement(audio_src_name);
        CreateAudioPlaybackElement(audio_sink_name);

        GstElement *video_src;
        
        if( video_src_name.length() != 0)
        {
            CreateVideoOutputElements();
            CreateVideoInputElement(video_src_name);
        }

        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        status_ = StatusConnecting;
        emit statusChanged(status_);
    }

    FarsightChannel::~FarsightChannel()
    {
        // delete widgets
        //if (video_preview_widget_)
        //    SAFE_DELETE(video_preview_widget_);

        //if (video_remote_output_widget_)
        //    SAFE_DELETE(video_remote_output_widget_);

        // TODO: CHECK Proper cleanup with unref
        if (tf_channel_)
        {
            //g_signal_handler_disconnect(tf_channel_, handler_id);
//            g_object_unref(tf_channel_);
            tf_channel_ = 0;
        }
        if (bus_) {
            g_object_unref(bus_);
            bus_ = 0;
        }
        gst_element_set_state(pipeline_, GST_STATE_NULL);

        if (video_input_bin_)
        {
            g_object_unref(video_input_bin_);
            video_input_bin_ = 0;
        }
        if (pipeline_) {
            g_object_unref(pipeline_);
            pipeline_ = 0;
        }
        if (audio_input_) {
            g_object_unref(audio_input_);
            audio_input_ = 0;
        }

        if (audio_playback_bin_){
            g_object_unref(audio_playback_bin_);
            audio_playback_bin_ = 0;
        }

        //if (audio_output_) {
        //    g_object_unref(audio_output_);
        //    audio_output_ = 0;
        //}
    }


    void FarsightChannel::CreateTfChannel()
    {
        try
        {
            tf_channel_ = createFarsightChannel(tp_channel_);        
        }
        catch(...)
        {
            throw Core::Exception("Cannot create TfChannel object!");
        }
        if (!tf_channel_)
        {
            LogError("Unable to construct TfChannel");
            return;
        }

        /* Set up the telepathy farsight channel */
        g_signal_connect(tf_channel_, "closed", G_CALLBACK(&FarsightChannel::onClosed), this);
        
        g_signal_connect(tf_channel_, "session-created", G_CALLBACK(&FarsightChannel::onSessionCreated), this);
        g_signal_connect(tf_channel_, "stream-created", G_CALLBACK(&FarsightChannel::onStreamCreated), this);
    }

    void FarsightChannel::CreatePipeline()
    {
        pipeline_ = gst_pipeline_new(NULL);
        if (pipeline_ == 0)
            throw Core::Exception("Cannot create GStreamer pipeline.");
        bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
        if (bus_ == 0)
            throw Core::Exception("Cannot create GStreamer bus.");
    }

    void FarsightChannel::CreateAudioInputElement(const QString & name)
    {
        audio_input_ = setUpElement(name);
        if (audio_input_ == 0)
            throw Core::Exception("Cannot create GStreamer audio input element.");
    }


    void FarsightChannel::CreateAudioPlaybackElement(const QString &audio_sink_name)
    {
        audio_playback_bin_ = gst_bin_new("audio-output-bin");
        if (audio_playback_bin_ == 0)
            throw Core::Exception("Cannot create GStreamer bin for audio playback.");

        fake_audio_output_ = setUpElement("fakesink");
        if (fake_audio_output_ == 0)
            throw Core::Exception("Cannot create GStreamer fake audio output element.");
        else
        {
            g_signal_connect(fake_audio_output_, "handoff", G_CALLBACK(&FarsightChannel::OnFakeSinkHandoff), this);
            g_object_set(G_OBJECT(fake_audio_output_), "signal-handoffs", (gboolean)true, this);
        }
        //return;
        // We use fake audio sink for now
        //audio_output_ = setUpElement(audio_sink_name);
        //if (audio_output_ == 0)
        //    throw Core::Exception("Cannot create GStreamer audio output element.");

        // audio modifications
        audio_resample_ = gst_element_factory_make("audioresample", NULL);
        if (audio_resample_ == 0)
            throw Core::Exception("Cannot create GStreamer audio resample element.");

        audio_capsfilter_ = gst_element_factory_make("capsfilter", NULL);
        GstCaps *audio_caps = gst_caps_new_simple("audio/x-raw-int",
            "channels", G_TYPE_INT, 1,
            "width", G_TYPE_INT, 16,
//            "depth", G_TYPE_INT, 16,
            "rate", G_TYPE_INT, 16000,
            "signed", G_TYPE_BOOLEAN, true,
//            "endianess", G_TYPE_INT, 1234,
            NULL);
        g_object_set(G_OBJECT(audio_capsfilter_), "caps", audio_caps, NULL);

        audio_convert_ = gst_element_factory_make("audioconvert", NULL);
        if (audio_convert_ == 0)
            throw Core::Exception("Cannot create GStreamer audio convert element.");

        gst_bin_add_many(GST_BIN(audio_playback_bin_),  fake_audio_output_, NULL);
    //    gboolean ok = gst_element_link_many( fake_audio_output_, NULL);
        //gst_bin_add_many(GST_BIN(audio_playback_bin_), audio_resample_, audio_capsfilter_, fake_audio_output_, NULL);
        //gboolean ok = gst_element_link_many(audio_resample_, audio_capsfilter_, fake_audio_output_, NULL);
        //if (!ok)
        //{
        //    QString error_message = "Cannot link elements for audio playback bin.";
        //    LogError(error_message.toStdString());
        //    throw Core::Exception(error_message.toStdString().c_str());
        //}

        // add ghost pad to audio_bin_
        GstPad *sink = gst_element_get_static_pad(fake_audio_output_, "sink");
        audio_playback_bin_sink_pad_ = gst_ghost_pad_new("sink", sink);
        gst_element_add_pad(GST_ELEMENT(audio_playback_bin_), audio_playback_bin_sink_pad_);
        gst_object_unref(G_OBJECT(sink));
        gst_object_ref(audio_playback_bin_);
        gst_object_sink(audio_playback_bin_);
    }

    void FarsightChannel::CreateVideoInputElement(const QString &video_src_name)
    {
        video_input_bin_ = gst_bin_new("video-input-bin");
        
        GstElement *scale = gst_element_factory_make("videoscale", NULL);
        GstElement *rate = gst_element_factory_make("videorate", NULL);
        GstElement *colorspace = gst_element_factory_make("ffmpegcolorspace", NULL);
        GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
        GstCaps *caps = gst_caps_new_simple("video/x-raw-yuv",
                "width", G_TYPE_INT, 320,
                "height", G_TYPE_INT, 240,
                NULL);
        g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);

        video_input_ = setUpElement(video_src_name);
        gst_bin_add_many(GST_BIN(video_input_bin_), video_input_, scale, rate, colorspace, capsfilter, NULL);
        gst_element_link_many(video_input_, scale, rate, colorspace, capsfilter, NULL);
        GstPad *src = gst_element_get_static_pad(capsfilter, "src");
        GstPad *ghost = gst_ghost_pad_new("src", src);
        Q_ASSERT(gst_element_add_pad(GST_ELEMENT(video_input_bin_), ghost));
        gst_object_unref(G_OBJECT(src));
        gst_object_ref(video_input_bin_);
        gst_object_sink(video_input_bin_);

        video_tee_ = setUpElement("tee");
        gst_object_ref(video_tee_);
        gst_object_sink(video_tee_);

        gst_bin_add_many(GST_BIN(pipeline_), video_input_bin_, video_tee_, video_preview_element_, NULL);
        gst_element_link_many(video_input_bin_, video_tee_, video_preview_element_, NULL);
    }

    void FarsightChannel::CreateVideoOutputElements()
    {
        video_preview_widget_ = new VideoWidget(bus_);
        video_preview_element_ = video_preview_widget_->GetVideoSink();

        video_remote_output_widget_ = new VideoWidget(bus_);
        video_remote_output_element_ = video_remote_output_widget_->GetVideoSink();
    }


    void FarsightChannel::OnFakeSinkHandoff(GstElement *fakesink, GstBuffer *buffer, GstPad *pad, gpointer user_data)
    {
        FarsightChannel* self = (FarsightChannel*)user_data;
        
        int offset;
        //if (!GST_BUFFER_OFFSET_IS_VALID(buffer))
        //    offset = 0;
        //else
        //    offset = (~GST_BUFFER_OFFSET(buffer))+1; // Two's complement

        //int offset_end;
        //if(!GST_BUFFER_OFFSET_END_IS_VALID(buffer))
        //    offset_end = buffer->size-1;
        //else
        //    offset_end = (~GST_BUFFER_OFFSET_END(buffer))+1; // Two's complement
        
        offset = 0;
        emit self->AudioPlaybackBufferReady(buffer->data + offset, buffer->size - offset);
    }


    void FarsightChannel::ConnectTfChannelEvents()
    {
    }

    GstElement* FarsightChannel::setUpElement(QString elemName)
    {
        std::string tempElemName = elemName.toStdString();
        const gchar* cStrElemName = tempElemName.c_str();
        GstElement* element = gst_element_factory_make(cStrElemName, NULL);
        gst_object_ref(element);
        gst_object_sink(element);
        return element;
    }

    FarsightChannel::Status FarsightChannel::GetStatus() const
    {
        return status_;
    }
    
    void FarsightChannel::SetAudioPlaybackVolume(const double value)
    {
        if(value<0||value>1)
        {
            LogError("Trying to set volume out of range");
            return;
        }
        double setValue = value*10; // range is from 0 to 10
        //g_value_set_double(&volume_, setValue); 
        //g_object_set_property (G_OBJECT(audio_volume_), "volume", &volume_);
    }
    
    void FarsightChannel::SetAudioRecordVolume(const double value)
    {
        if(value<0||value>1){
            LogError("Trying to set record volume out of range");
            return;
        }        
        // todo: Implement
    }

    // Link bus events to tf_channel_
    gboolean FarsightChannel::busWatch(GstBus *bus, GstMessage *message, FarsightChannel *self)
    {
        try
        {
            if(self->tf_channel_ == NULL) 
            {
                LogWarning("CommunicationModule: receiving bus message when tf_channel_ is NULL");
                return FALSE;
            }
            tf_channel_bus_message(self->tf_channel_, message);
            return TRUE;
        } catch(...)
        {
            LogWarning("CommunicationModule: passing gstreamer bus message to telepathy-farsight failed");
            return FALSE;
        }
    }

    void FarsightChannel::onClosed(TfChannel *tfChannel, FarsightChannel *self)
    {
        self->status_ = StatusDisconnected;
        emit self->statusChanged(self->status_);
    }

    void FarsightChannel::onSessionCreated(TfChannel *tfChannel, FsConference *conference, FsParticipant *participant, FarsightChannel *self)
    {
        gst_bus_add_watch(self->bus_, (GstBusFunc) &FarsightChannel::busWatch, self);
        gst_bin_add(GST_BIN(self->pipeline_), GST_ELEMENT(conference));
        gst_element_set_state(GST_ELEMENT(conference), GST_STATE_PLAYING);
    }

    void FarsightChannel::onStreamCreated(TfChannel *tfChannel, TfStream *stream, FarsightChannel *self)
    {
        guint media_type;
        GstPad *sink;

        g_signal_connect(stream, "src-pad-added", G_CALLBACK(&FarsightChannel::onSrcPadAdded), self);
        g_signal_connect(stream, "request-resource", G_CALLBACK(&FarsightChannel::onRequestResource), NULL);

        g_object_get(stream, "media-type", &media_type, "sink-pad", &sink, NULL);

        GstPad *pad;

        switch (media_type)
        {
        case TP_MEDIA_STREAM_TYPE_AUDIO:
            gst_bin_add(GST_BIN(self->pipeline_), self->audio_input_);
            gst_element_set_state(self->audio_input_, GST_STATE_PLAYING);
            pad = gst_element_get_static_pad(self->audio_input_, "src");
            gst_pad_link(pad, sink);
            break;
        case TP_MEDIA_STREAM_TYPE_VIDEO:
            pad = gst_element_get_request_pad(self->video_tee_, "src%d");
            gst_pad_link(pad, sink);
            break;
        default:
            Q_ASSERT(false);
        }

        gst_object_unref(sink);
    }

    void FarsightChannel::onSrcPadAdded(TfStream *stream, GstPad *src_pad, FsCodec *codec, FarsightChannel *self)
    {           
        // todo: Check if source pad is already linked!
        gint clock_rate = codec->clock_rate;
        self->audio_stream_in_clock_rate_ = clock_rate;
        gint channel_count = codec->channels;

        guint media_type;

        g_object_get(stream, "media-type", &media_type, NULL);

        GstPad *output_pad;
        GstElement *output_element = 0;

        bool sink_already_linked = false;

        switch (media_type)
        {
        case TP_MEDIA_STREAM_TYPE_AUDIO:
             output_element = self->audio_playback_bin_;
//            g_object_ref(output_element); // do we need this
              if (self->audio_in_src_pad_)
                sink_already_linked = true;
            break;
        case TP_MEDIA_STREAM_TYPE_VIDEO:
            output_element = self->video_remote_output_element_;
            if (self->video_in_src_pad_)
                sink_already_linked = true;
            break;
        default:
            Q_ASSERT(false);
        }

        if (sink_already_linked)
        {

        }
        else
        {
            gst_bin_add(GST_BIN(self->pipeline_), output_element);
        }

        output_pad = gst_element_get_static_pad(output_element, "sink");
        switch (media_type)
        {
        case TP_MEDIA_STREAM_TYPE_AUDIO:
            if (self->audio_in_src_pad_)
            {
                gst_pad_unlink(self->audio_in_src_pad_, output_pad);
                self->audio_in_src_pad_ = src_pad;
            }
                break;
        case TP_MEDIA_STREAM_TYPE_VIDEO:
            if (self->video_in_src_pad_)
            {
                gst_pad_unlink(self->video_in_src_pad_, output_pad);
                self->video_in_src_pad_ = src_pad;
            }
                break;
        }
        gst_pad_link(src_pad, output_pad);
        gst_element_set_state(output_element, GST_STATE_PLAYING);

        self->status_ = StatusConnected;
        emit self->statusChanged(self->status_);
    }

    gboolean FarsightChannel::onRequestResource(TfStream *stream, guint direction, gpointer data)
    {
        LogInfo("CommunicationModule: resource request");
        return TRUE;
    }
} // end of namespace: TelepathyIM
