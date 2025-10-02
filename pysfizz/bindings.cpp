#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/ndarray.h>
#include <sfizz.hpp>
#include <sfizz/Synth.h>
#include <sfizz/Region.h>
#include <sfizz/Defaults.h>
#include <sfizz/sfizz_private.hpp>
#include <sfizz/SynthConfig.h>

namespace nb = nanobind;

class Synth {
private:
    sfz::Sfizz synth_;
    sfizz_synth_t* synth_handle_;
    std::vector<float> leftBuffer_;
    std::vector<float> rightBuffer_;
    int sampleRate_;
    int blockSize_;
    
public:
    // CONSTRUCTOR: Initialize synth with audio configuration
    // Based on sfizz Config.h: defaultSampleRate=48000, defaultSamplesPerBlock=1024
    Synth(int sampleRate = 48000, int blockSize = 1024) 
        : sampleRate_(sampleRate), blockSize_(blockSize) {
        
        // Cache handle once in constructor
        synth_handle_ = synth_.handle();
        if (!synth_handle_) {
            throw std::runtime_error("Failed to get synth handle");
        }
        
        // Configure synth audio settings (from Synth.cpp setSampleRate/setSamplesPerBlock)
        synth_.setSampleRate(sampleRate);
        synth_.setSamplesPerBlock(blockSize);
        
        // Allocate stereo buffers for rendering
        leftBuffer_.resize(blockSize);
        rightBuffer_.resize(blockSize);
    }
    
    // === PARSER METHODS ===
    
    // Load SFZ file into the synth's internal parser
    // Based on sfizz Synth.cpp loadSfzFile() method
    bool loadSfzFile(const std::string& path) {
        return synth_.loadSfzFile(path);
    }
    
    // Get number of regions parsed from SFZ file
    // Based on sfizz Synth.cpp getNumRegions() method
    int getNumRegions() const {
        return synth_.getNumRegions();
    }
    
    // Get detailed region data for analysis
    // Based on sfizz Region.h and SynthPrivate.h region access
    std::map<std::string, nb::object> getRegionData(int regionIndex) const {
        if (regionIndex < 0 || regionIndex >= synth_.getNumRegions()) {
            throw nb::value_error("Region index out of range");
        }
        
        const auto* region = synth_handle_->synth.getRegionView(regionIndex);
        if (!region) {
            throw nb::value_error("Failed to access region");
        }
        
        std::map<std::string, nb::object> region_data;
        
        // ============================================================================
        // BASIC REGION INFORMATION
        // ============================================================================
        
        // Unique identifier for this region (0-based index)
        region_data["id"] = nb::int_(region->getId().number());
        
        // Sample filename (e.g., "piano_C4.wav", "*sine", "*silence")
        // Note: "*" prefix indicates generated samples (sine wave, silence, etc.)
        region_data["sample_id"] = nb::str(region->sampleId->filename().c_str());
        
        // ============================================================================
        // KEY MAPPING (MIDI Note Numbers: 0-127)
        // ============================================================================
        
        // Lowest MIDI note number that triggers this region (default: 0)
        // Range: 0-127, where 60 = middle C
        region_data["lokey"] = nb::int_(region->keyRange.getStart());
        
        // Highest MIDI note number that triggers this region (default: 127)
        // Range: 0-127, where 60 = middle C
        region_data["hikey"] = nb::int_(region->keyRange.getEnd());
        
        // Pitch keycenter - the root note of the sample (default: 60 = middle C)
        // This is the note that plays at the sample's original pitch
        // Range: 0-127, where 60 = middle C
        region_data["key"] = nb::int_(region->pitchKeycenter);
        
        // ============================================================================
        // VELOCITY MAPPING (MIDI Velocity: 0-127)
        // ============================================================================
        
        // Lowest velocity value that triggers this region (default: 0)
        // Range: 0.0-127.0 (normalized MIDI velocity)
        region_data["lovel"] = nb::float_(region->velocityRange.getStart());
        
        // Highest velocity value that triggers this region (default: 127)
        // Range: 0.0-127.0 (normalized MIDI velocity)
        region_data["hivel"] = nb::float_(region->velocityRange.getEnd());
        
        // ============================================================================
        // PITCH INFORMATION
        // ============================================================================
        
        // Pitch keycenter - same as "key" above (default: 60)
        // This is the reference note for pitch calculations
        region_data["pitch_keycenter"] = nb::int_(region->pitchKeycenter);
        
        // Pitch tracking per key - how much pitch changes per semitone (default: 100)
        // 100 cents = 1 semitone, 1200 cents = 1 octave
        // Positive values: higher keys = higher pitch
        // Negative values: higher keys = lower pitch (rare)
        region_data["pitch_keytrack"] = nb::float_(region->pitchKeytrack);
    
        // Random pitch variation - adds natural pitch variation to each note (default: 0)
        // Range: 0-12000 cents (0 = no variation, 100 = 1 semitone variation)
        // Creates human-like pitch variation for more natural sound
        region_data["pitch_random"] = nb::float_(region->pitchRandom);

        // Pitch tracking per velocity - how much pitch changes with velocity (default: 0)
        // Positive values: higher velocity = higher pitch
        // Negative values: higher velocity = lower pitch
        region_data["pitch_veltrack"] = nb::float_(region->pitchVeltrack);
        
        // Transpose - pitch shift in semitones (default: 0)
        // Range: typically -12 to +12 semitones
        // Positive values: pitch up, negative values: pitch down
        region_data["transpose"] = nb::float_(region->transpose);
        
        // Fine tuning - pitch adjustment in cents (default: 0)
        // Range: typically -100 to +100 cents
        // Used for fine-tuning the sample's pitch
        region_data["tune"] = nb::float_(region->pitch);
        
        // PITCH CALCULATION RELATIONSHIP (in cents):
        // pitchVariationInCents = pitch_keytrack * (noteNumber - pitch_keycenter)  // note difference
        //                       + tune                                             // sample tuning (region_data['tune'])
        //                       + 100 * transpose                                  // transpose opcode (region_data['transpose'])
        //                       + velocity * pitch_veltrack                        // velocity tracking (region_data['pitch_veltrack'])
        //                       + random(0, pitch_random)                          // random pitch variation (region_data['pitch_random'])
        // Where: noteNumber = MIDI note pressed (0-127)
        
        // ============================================================================
        // SUSTAIN PEDAL INFORMATION
        // ============================================================================
        
        // Whether this region responds to sustain pedal (default: true)
        // true = region respects sustain pedal (CC 64 by default)
        // false = region ignores sustain pedal completely
        region_data["check_sustain"] = nb::bool_(region->checkSustain);
        
        // MIDI CC number for sustain control (default: 64)
        // 64 = standard MIDI sustain pedal
        // Can be set to any CC number (0-127) for custom sustain control
        region_data["sustain_cc"] = nb::int_(region->sustainCC);
        
        // SUSTAIN BEHAVIOR:
        // - When check_sustain=true: Notes continue playing after key release if sustain pedal is pressed
        // - When check_sustain=false: Notes stop immediately when key is released (no pedal effect)
        // - Natural sustain (from sample envelope/loops) works regardless of check_sustain setting
        
        // ============================================================================
        // LOOP INFORMATION
        // ============================================================================
        
        // Loop mode - how the sample plays back (default: "no_loop")
        // Based on sfizz Defaults.cpp: loopMode { LoopMode::no_loop, ... }
        // From sfizz Opcode.cpp: enum class LoopMode { no_loop = 0, one_shot, loop_continuous, loop_sustain }
        std::string loopModeStr;
        if (region->loopMode.has_value()) {
            switch (region->loopMode.value()) {
                case sfz::LoopMode::no_loop: loopModeStr = "no_loop"; break;
                case sfz::LoopMode::one_shot: loopModeStr = "one_shot"; break;
                case sfz::LoopMode::loop_continuous: loopModeStr = "loop_continuous"; break;
                case sfz::LoopMode::loop_sustain: loopModeStr = "loop_sustain"; break;
            }
        } else {
            loopModeStr = "no_loop"; // Default when not specified (from Defaults.cpp line 210)
        }
        region_data["loop_mode"] = nb::str(loopModeStr.c_str());
        
        // LOOP MODE BEHAVIORS:
        // - "no_loop": Sample plays from start to end OR until note-off, whichever comes first
        // - "one_shot": Sample plays from start to end, completely ignoring note-off events (common for drums)
        // - "loop_continuous": Sample loops continuously from loop_start to loop_end
        // - "loop_sustain": Sample loops only while key is held down
        
        // ============================================================================
        // TRIGGER INFORMATION (Mutually Exclusive)
        // ============================================================================
        
        // Trigger type - when this region activates (default: "attack")
        // Based on sfizz Defaults.cpp: trigger { Trigger::attack, ... }
        // From sfizz Defaults.h: enum class Trigger { attack = 0, release, release_key, first, legato }
        std::string triggerStr;
        switch (region->trigger) {
            case sfz::Trigger::attack: triggerStr = "attack"; break;
            case sfz::Trigger::release: triggerStr = "release"; break;
            case sfz::Trigger::release_key: triggerStr = "release_key"; break;
            case sfz::Trigger::first: triggerStr = "first"; break;
            case sfz::Trigger::legato: triggerStr = "legato"; break;
        }
        region_data["trigger"] = nb::str(triggerStr.c_str());
        
        // TRIGGER BEHAVIORS (Only one can be active per region):
        // - "attack": Triggers when note is pressed (normal playback)
        // - "release": Triggers when note is released AND sustain pedal is pressed
        // - "release_key": Triggers when note is released (regardless of sustain pedal)
        // - "first": Triggers only on the first note when no other notes are playing
        // - "legato": Triggers only on subsequent notes when other notes are already playing
        
        // ============================================================================
        // SAMPLE PLAYBACK INFORMATION
        // ============================================================================
        
        // Sample start offset in samples (default: 0)
        // Range: 0 to sample length
        region_data["offset"] = nb::int_(region->offset);
        
        // Sample end position in samples (default: end of file)
        // Range: 0 to sample length, must be > offset
        region_data["end"] = nb::int_(region->sampleEnd);
        
        // Sample count/length in samples (optional)
        // When specified, overrides natural sample length
        if (region->sampleCount.has_value()) {
            region_data["count"] = nb::int_(region->sampleCount.value());
        } else {
            region_data["count"] = nb::none(); // No count specified
        }
        
        // Loop start position in samples (default: 0)
        region_data["loop_start"] = nb::int_(region->loopRange.getStart());
        
        // Loop end position in samples (default: end of sample)
        region_data["loop_end"] = nb::int_(region->loopRange.getEnd());
        
        // Loop count/length in samples (optional)
        // When specified, limits number of loop iterations
        if (region->loopCount.has_value()) {
            region_data["loop_count"] = nb::int_(region->loopCount.value());
        } else {
            region_data["loop_count"] = nb::none(); // No loop count specified
        }
        
        // ============================================================================
        // AMPLITUDE/GAIN INFORMATION
        // ============================================================================
        
        // Volume level in dB (default: 0.0)
        // Range: typically -96.0 to +12.0 dB
        region_data["volume"] = nb::float_(region->volume);
        
        // Amplitude level (default: 100.0)
        // Range: typically 0.0 to 100.0
        region_data["amplitude"] = nb::float_(region->amplitude);
        
        // Gain level in dB (default: 0.0)
        // Range: typically -96.0 to +12.0 dB
        region_data["gain"] = nb::float_(region->getBaseGain());
        
        // ============================================================================
        // EFFECTS INFORMATION
        // ============================================================================
        
        // Pan position (default: 0.0 = center)
        // Range: -100.0 (left) to +100.0 (right)
        region_data["pan"] = nb::float_(region->pan);
        
        // Stereo width (default: 100.0 = full stereo)
        // Range: 0.0 (mono) to 100.0 (full stereo)
        region_data["width"] = nb::float_(region->width);
        
        // Position in stereo field (default: 0.0 = center)
        // Range: -100.0 (left) to +100.0 (right)
        region_data["position"] = nb::float_(region->position);
        
        return region_data;
    }
    
    // Get region indices that respond to a specific MIDI note
    // Based on sfizz Synth.cpp note activation lists
    std::vector<int> getRegionsForNote(int midiNote) const {
        if (midiNote < 0 || midiNote > 127) {
            throw nb::value_error("MIDI note must be between 0 and 127");
        }
        
        std::vector<int> regions;
        const int numRegions = synth_handle_->synth.getNumRegions();
        for (int i = 0; i < numRegions; ++i) {
            const auto* region = synth_handle_->synth.getRegionView(i);
            if (region && region->keyRange.containsWithEnd(midiNote)) {
                regions.push_back(i);
            }
        }
        
        return regions;
    }
    
    // === SYNTHESIS METHODS ===
    
    // Send MIDI Note On event to trigger voices
    // Based on sfizz Synth.cpp noteOn() method
    void noteOn(int delay, int noteNumber, int velocity) {
        if (noteNumber < 0 || noteNumber > 127) {
            throw nb::value_error("Note number must be between 0 and 127");
        }
        if (velocity < 0 || velocity > 127) {
            throw nb::value_error("Velocity must be between 0 and 127");
        }
        
        synth_handle_->synth.noteOn(delay, noteNumber, velocity);
    }
    
    // Send MIDI Note Off event to release voices
    // Based on sfizz Synth.cpp noteOff() method
    void noteOff(int delay, int noteNumber, int velocity = 0) {
        if (noteNumber < 0 || noteNumber > 127) {
            throw nb::value_error("Note number must be between 0 and 127");
        }
        if (velocity < 0 || velocity > 127) {
            throw nb::value_error("Velocity must be between 0 and 127");
        }
        
        synth_handle_->synth.noteOff(delay, noteNumber, velocity);
    }
    
    // Send MIDI Control Change event
    // Based on sfizz Synth.cpp cc() method
    // 
    // IMPORTANT: Values are "held" until a new value is received!
    // No interpolation happens between values - each value persists
    // until the next timestamp.
    //
    // ADVANCED USAGE: Temporal automation requires custom implementation.
    //
    // Example timeline:
    // synth.cc(0, 7, 64)     → Volume = 64 (held until 500ms)
    // synth.cc(500, 7, 80)   → Volume = 80 (held until 1000ms)  
    // synth.cc(1000, 7, 96)  → Volume = 96 (held until 1500ms)
    //
    // This creates "step automation" - values jump between timestamps.
    // For smooth automation, users must implement their own
    // interpolation algorithms (e.g., LFO, envelope generators, etc.)
    //
    void cc(int delay, int ccNumber, int value) {
        if (ccNumber < 0 || ccNumber > 127) {
            throw nb::value_error("CC number must be between 0 and 127");
        }
        if (value < 0 || value > 127) {
            throw nb::value_error("CC value must be between 0 and 127");
        }
        
        synth_handle_->synth.cc(delay, ccNumber, value);
    }
    
    // Send MIDI Pitch Wheel event
    // Based on sfizz Synth.cpp pitchWheel() method
    // 
    // IMPORTANT: Values are "held" until a new value is received!
    // No interpolation happens between values - each value persists
    // until the next timestamp.
    //
    // ADVANCED USAGE: Temporal automation requires custom implementation.
    //
    // Example timeline:
    // synth.pitch_wheel(0, 0)      → Pitch = 0 (held until 500ms)
    // synth.pitch_wheel(500, 1000) → Pitch = 1000 (held until 1000ms)
    // synth.pitch_wheel(1000, 0)   → Pitch = 0 (held until 1500ms)
    //
    // This creates "step automation" - values jump between timestamps.
    // For smooth automation, users must implement their own
    // interpolation algorithms (e.g., LFO, envelope generators, etc.)
    //
    void pitchWheel(int delay, int pitch) {
        if (pitch < -8192 || pitch > 8192) {
            throw nb::value_error("Pitch wheel value must be between -8192 and +8192");
        }
        
        synth_handle_->synth.pitchWheel(delay, pitch);
    }
    
    // Render one audio block (stereo output)
    // Based on sfizz Synth.cpp renderBlock() method
    // Returns NumPy arrays
    nb::tuple renderBlock() {
        // Create AudioSpan for stereo rendering (from sfizz AudioSpan usage)
        float* buffers[2] = { leftBuffer_.data(), rightBuffer_.data() };
        sfz::AudioSpan<float> bufferSpan { buffers, 2, 0, static_cast<size_t>(blockSize_) };
        
        // Render audio block (clears buffer, processes voices, applies effects)
        synth_handle_->synth.renderBlock(bufferSpan);
        
        // return NumPy array
        auto left = nb::ndarray<nb::numpy, float>(leftBuffer_.data(), {leftBuffer_.size()});
        auto right = nb::ndarray<nb::numpy, float>(rightBuffer_.data(), {rightBuffer_.size()});
        return nb::make_tuple(left, right);
    }
    
    // === SYNTH CONFIGURATIONS ===

    // Get sample rate
    int getSampleRate() const {
        return sampleRate_;
    }
    
    // Set sample rate
    // Based on sfizz Synth.cpp setSampleRate() method
    void setSampleRate(int sampleRate) {
        if (sampleRate <= 0) {
            throw nb::value_error("Sample rate must be positive");
        }
        
        sampleRate_ = sampleRate;
        synth_.setSampleRate(sampleRate);
    }
    
    // Get block size
    int getBlockSize() const {
        return blockSize_;
    }

    // Set block size
    // Based on sfizz Synth.cpp setSamplesPerBlock() method
    void setBlockSize(int blockSize) {
        if (blockSize <= 0) {
            throw nb::value_error("Block size must be positive");
        }
        
        blockSize_ = blockSize;
        synth_.setSamplesPerBlock(blockSize);
        
        // Reallocate buffers
        leftBuffer_.resize(blockSize);
        rightBuffer_.resize(blockSize);
    }
    
    // Set number of voices (polyphony limit).
    void setNumVoices(int numVoices) {
        if (numVoices <= 0) {
            throw nb::value_error("Number of voices must be positive");
        }
        
        synth_handle_->synth.setNumVoices(numVoices);
    }
    
    // Get number of voices (polyphony limit).
    int getNumVoices() const {
        return synth_handle_->synth.getNumVoices();
    }

    // Get number of active voices (currently playing or in release phase).
    int getNumActiveVoices() const {
        return synth_handle_->synth.getNumActiveVoices();
    }

    // === OFFLINE ACCELERATION METHODS ===

    // Check if freewheeling is enabled
    bool isFreeWheeling() const {
        const auto& synthConfig = synth_handle_->synth.getResources().getSynthConfig();
        return synthConfig.freeWheeling;
    }
    
    // Enable freewheeling mode for offline rendering
    // Based on sfizz Synth.cpp enableFreeWheeling() method
    void enableFreeWheeling() {
        synth_handle_->synth.enableFreeWheeling();
    }
    
    // Disable freewheeling mode for real-time use
    // Based on sfizz Synth.cpp disableFreeWheeling() method
    void disableFreeWheeling() {
        synth_handle_->synth.disableFreeWheeling();
    }
    
    // Get sample quality
    int getSampleQuality() const {
        const auto& synthConfig = synth_handle_->synth.getResources().getSynthConfig();
        return synthConfig.currentSampleQuality();
    }
    
    // Get oscillator quality
    int getOscillatorQuality() const {
        const auto& synthConfig = synth_handle_->synth.getResources().getSynthConfig();
        return synthConfig.currentOscillatorQuality();
    }

    // Set sample quality
    void setSampleQuality(int quality) {
        if (quality < 0 || quality > 10) {
            throw nb::value_error("Sample quality must be between 0 and 10");
        }
        
        synth_handle_->synth.setSampleQuality(
            isFreeWheeling() ? sfz::Synth::ProcessMode::ProcessFreewheeling : sfz::Synth::ProcessMode::ProcessLive,
            quality
        );
    }

    // Set oscillator quality
    void setOscillatorQuality(int quality) {
        if (quality < 0 || quality > 3) {
            throw nb::value_error("Oscillator quality must be between 0 and 3");
        }
        
        synth_handle_->synth.setOscillatorQuality(
            isFreeWheeling() ? sfz::Synth::ProcessMode::ProcessFreewheeling : sfz::Synth::ProcessMode::ProcessLive,
            quality
        );
    }

};

// === NANOBIND MODULE DEFINITION ===
NB_MODULE(_sfizz, m) {

    // Bind the unified Synth class
    nb::class_<Synth>(m, "Synth")
        // Constructor
        .def(nb::init<int, int>(), nb::arg("sample_rate") = 48000, nb::arg("block_size") = 1024)
        
        // Parser methods
        .def("load_sfz_file", &Synth::loadSfzFile)
        .def("get_num_regions", &Synth::getNumRegions)
        .def("get_region_data", &Synth::getRegionData)
        .def("get_regions_for_note", &Synth::getRegionsForNote)
        
        // MIDI input methods
        .def("note_on", &Synth::noteOn)
        .def("note_off", &Synth::noteOff)
        .def("cc", &Synth::cc)
        .def("pitch_wheel", &Synth::pitchWheel)
        
        // Audio rendering
        .def("render_block", &Synth::renderBlock)
        
        // Configuration methods
        .def("get_sample_rate", &Synth::getSampleRate)
        .def("set_sample_rate", &Synth::setSampleRate)

        .def("get_block_size", &Synth::getBlockSize)
        .def("set_block_size", &Synth::setBlockSize)

        .def("get_num_voices", &Synth::getNumVoices)
        .def("set_num_voices", &Synth::setNumVoices)

        .def("get_num_active_voices", &Synth::getNumActiveVoices)

        // Offline acceleration methods
        .def("is_freewheeling", &Synth::isFreeWheeling)
        .def("enable_freewheeling", &Synth::enableFreeWheeling)
        .def("disable_freewheeling", &Synth::disableFreeWheeling)

        .def("get_sample_quality", &Synth::getSampleQuality)
        .def("get_oscillator_quality", &Synth::getOscillatorQuality)

        .def("set_sample_quality", &Synth::setSampleQuality)
        .def("set_oscillator_quality", &Synth::setOscillatorQuality);
}
