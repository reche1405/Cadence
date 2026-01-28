#ifndef LINUXBACKEND_H
#define LINUXBACKEND_H

#include "../../common/audiobackend.h"
namespace AudioEngine {

class LinuxBackend : public AudioEngine::IAudioBackend
{
public:
    LinuxBackend();
};

} // namespace AudioEngine

#endif // LINUXBACKEND_H
