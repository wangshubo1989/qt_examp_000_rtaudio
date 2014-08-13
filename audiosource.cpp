// C++ Standard includes
#include <iterator>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <mutex>

// Stk includes
#include "RtAudio.h"

// App includes
#include "audiosource.h"
#include "parameters.h"


bool audioSource::log_buff;
bool audioSource::save_buff_log;
std::mutex foo, boo;

static int signal_iter = 0;
static QVector<QPointF>    m_qpoint_signal( AUDIO_DEV_BUFFER_FRAMES_NBR );


int audioSource::audio_buffer_full( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                                double streamTime, RtAudioStreamStatus status, void *data )
{
    // Since the number of input and output channels is equal, we can do
    // a simple buffer copy operation here.
    if ( status ) std::cout << "Stream over/underflow detected." << std::endl;

    logDataInBuff( inputBuffer );

    std::lock (foo, boo);
    for(; signal_iter < AUDIO_DEV_BUFFER_FRAMES_NBR; signal_iter++)
    {
        ::m_qpoint_signal[signal_iter] = QPointF( signal_iter, ((double*)inputBuffer)[signal_iter] );
        //::m_qpoint_signal[signal_iter] = QPointF( signal_iter, ((double*)outputBuffer)[signal_iter] );
    }
    foo.unlock();
    boo.unlock();

    return 0;
}

audioSource::audioSource(unsigned int sampl_freq) : m_sampling_freq(sampl_freq)
{
    log_buff = false;
    save_buff_log = false;
    if ( m_audio_device.getDeviceCount() < 1 )
    {
        std::cout << "\nNo audio devices found!\n";
        exit( 0 );
    }

    m_aud_dev_in_params.nChannels = AUDIO_DEV_CHANNEL_NBR;
    m_audio_buffer_frames = AUDIO_DEV_BUFFER_FRAMES_NBR;

    // iDevice = 0;  -> Device Name = Default Device - duplex mode
    // iDevice = 5;  -> Device Name = Mic in at rear panel (Pink) (Realtek High Definition Audio)
    //                  !!! This majk has to be than set as default in the system 'control panel->sound->recording'
    // iDevice = 8;  -> Device Name = Microphone (3- USB Audio CODEC ) - external 'Alesis' mixpult
    unsigned int iDevice = 0, oDevice = 0;
    if ( iDevice == 0 )
        m_aud_dev_in_params.deviceId = m_audio_device.getDefaultInputDevice();
    else
        m_aud_dev_in_params.deviceId = iDevice - 1;

    m_aud_dev_in_params.firstChannel = AUDIO_DEV_IN_FISRT_CHANNEL_OFFSET;

    if ( oDevice == 0 )
        m_aud_dev_out_params.deviceId = m_audio_device.getDefaultOutputDevice();
    else
        m_aud_dev_out_params.deviceId = oDevice - 1;

    m_aud_dev_out_params.nChannels = AUDIO_DEV_CHANNEL_NBR;
    m_aud_dev_out_params.firstChannel = AUDIO_DEV_OUT_FISRT_CHANNEL_OFFSET;

    try {
        // !!! if want to use input and output and one of them is aquired from duplex device, second one has to be as well
        // first variant for input only mode
        m_audio_device.openStream( NULL,
                                   &m_aud_dev_in_params,
                                   FORMAT,
                                   m_sampling_freq,
                                   &m_audio_buffer_frames,
                                   &audio_buffer_full,
                                   (void *)&m_audio_buffer_bytes,
                                   &m_options );

        // second variant is a must for duplex mode
//         m_audio_device.openStream( &m_aud_dev_out_params, NULL, FORMAT, m_sampling_freq,
//                                    &m_audio_buffer_frames, &audio_buffer_full, (void *)&m_audio_buffer_bytes, &m_options );
    }
    catch ( RtAudioError& e ) {
        std::cout << '\n' << e.getMessage() << '\n' << std::endl;
        exit( 1 );
    }
    m_audio_buffer_bytes = m_audio_buffer_frames * m_aud_dev_in_params.nChannels * sizeof( MY_TYPE );

    // start audio stream
    try {
        m_audio_device.startStream();
    }
    catch ( RtAudioError& e ) {
        std::cout << '\n' << e.getMessage() << '\n' << std::endl;
    }
}

audioSource::~audioSource()
{
    if ( m_audio_device.isStreamRunning() )
    {
        m_audio_device.stopStream();
    }
}


const int row_elements = AUDIO_DEV_BUFFER_FRAMES_NBR;

void audioSource::setLogBuff()
{
    log_buff = true;
}

bool audioSource::getLogBuff()
{
    return log_buff;
}

void audioSource::setSaveBuffLog()
{
    save_buff_log = true;
}

const int row_nbr = 2;
static std::vector<std::vector<double>> audio_log( row_nbr );

void audioSource::logDataInBuff( void *inputBuffer )
{
    static int log_iter = 0;

    if( log_buff == true )
    {
        int iter = 0;
        for( ; iter < row_elements; iter++)
        {
            ::audio_log[ log_iter ].push_back(((double*)inputBuffer)[iter]);
        }

        log_iter++;
        if( log_iter == row_nbr )
        {
            log_iter = 0;
            log_buff = false;
            std::cout << "Finished sample logging ..." << std::endl;
        }
    }
}

void audioSource::fillSignal( QVector<QPointF> & vector )
{
    std::lock ( foo, boo );
    vector = ::m_qpoint_signal;
    foo.unlock();
    boo.unlock();
    signal_iter = 0;
}

void audioSource::storeDataToFile()
{
    if( save_buff_log == true )
    {
        std::ofstream out_curve;
        out_curve.open( "signal_samples.txt", std::ios::out );
        std::stringstream   ssline;
        ssline.setf( std::ios::fixed, std::ios::floatfield );
        ssline.precision( 6 );

        for( int iter = 0; iter < row_nbr; iter++ )
        {
            for( auto & index: ::audio_log[ iter ] )
            {
                //std::cout << index << std::endl;
                ssline << index;
                out_curve << ssline.str() << "\n";
                ssline.str( "" ) ;
            }
        }
        out_curve.close();
        save_buff_log = false;
        std::cout << "Finished storing data ..." << std::endl;
    }
}

