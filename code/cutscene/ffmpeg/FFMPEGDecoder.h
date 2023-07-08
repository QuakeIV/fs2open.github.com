#pragma once

#include "cutscene/Decoder.h"

namespace cutscene {
namespace ffmpeg {
struct InputStream;

struct DecoderStatus;

class FFMPEGDecoder: public Decoder {
 private:
	std::unique_ptr<InputStream> m_input;

	std::unique_ptr<DecoderStatus> m_status;

 public:
	FFMPEGDecoder();

	virtual ~FFMPEGDecoder();

	bool initialize(const SCP_string& fileName) override;

	MovieProperties getProperties() override;

	void startDecoding() override;

	bool hasAudio() override;

	void close() override;
};
}
}
