file(SHA256 "${PEQ_PLUGIN}" plugin_sha)
file(SHA256 "${PEQ_DSP}" dsp_sha)
file(WRITE "${PEQ_OUTPUT}" "{\n  \"abi\":1,\n  \"qtVersion\":\"6.11.2\",\n  \"sourceFingerprint\":\"${PEQ_SOURCE}\",\n  \"files\":{\"multimedia/ffmpegmediaplugin.dll\":\"${plugin_sha}\",\"RelaxerAudioDsp.dll\":\"${dsp_sha}\"}\n}\n")
