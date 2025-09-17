#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/optional.h>
#include <sfizz.hpp>
#include <sfizz/Synth.h>
#include <sfizz/Region.h>
#include <sfizz/Defaults.h>
#include <sfizz/sfizz_private.hpp>

namespace nb = nanobind;

class Parser {
private:
    sfz::Sfizz synth_;
    
public:
    Parser() = default;
    
    bool loadSfzFile(const std::string& path) {
        return synth_.loadSfzFile(path);
    }
    
    int getNumRegions() const {
        return synth_.getNumRegions();
    }
    
    // Get basic region data for a specific region index
    std::map<std::string, nb::object> getRegionData(int regionIndex) const {
        std::map<std::string, nb::object> regionData;
        
        auto* synth_handle = synth_.handle();
        if (!synth_handle) {
            throw nb::value_error("No SFZ file loaded. Call load_sfz_file() first.");
        }
        
        // Bounds checking with Python exception
        int numRegions = synth_handle->synth.getNumRegions();
        if (regionIndex < 0 || regionIndex >= numRegions) {
            std::string errorMsg = "Region index " + std::to_string(regionIndex) + 
                                 " out of range. Valid range: 0 to " + 
                                 std::to_string(numRegions - 1);
            throw nb::index_error(errorMsg.c_str());
        }
        
        const auto* region = synth_handle->synth.getRegionView(regionIndex);
        if (!region) {
            std::string errorMsg = "Failed to access region " + std::to_string(regionIndex);
            throw nb::value_error(errorMsg.c_str());
        }
        
        // ============================================================================
        // BASIC REGION INFORMATION
        // ============================================================================
        
        // Unique identifier for this region (0-based index)
        regionData["id"] = nb::int_(region->getId().number());
        
        // Sample filename (e.g., "piano_C4.wav", "*sine", "*silence")
        // Note: "*" prefix indicates generated samples (sine wave, silence, etc.)
        regionData["sample_id"] = nb::str(region->sampleId->filename().c_str());
        
        // ============================================================================
        // KEY MAPPING (MIDI Note Numbers: 0-127)
        // ============================================================================
        
        // Lowest MIDI note number that triggers this region (default: 0)
        // Range: 0-127, where 60 = middle C
        regionData["lokey"] = nb::int_(region->keyRange.getStart());
        
        // Highest MIDI note number that triggers this region (default: 127)
        // Range: 0-127, where 60 = middle C
        regionData["hikey"] = nb::int_(region->keyRange.getEnd());
        
        // Pitch keycenter - the root note of the sample (default: 60 = middle C)
        // This is the note that plays at the sample's original pitch
        // Range: 0-127, where 60 = middle C
        regionData["key"] = nb::int_(region->pitchKeycenter);
        
        // ============================================================================
        // VELOCITY MAPPING (MIDI Velocity: 0-127)
        // ============================================================================
        
        // Lowest velocity value that triggers this region (default: 0)
        // Range: 0.0-127.0 (normalized MIDI velocity)
        regionData["lovel"] = nb::float_(region->velocityRange.getStart());
        
        // Highest velocity value that triggers this region (default: 127)
        // Range: 0.0-127.0 (normalized MIDI velocity)
        regionData["hivel"] = nb::float_(region->velocityRange.getEnd());
        
        // ============================================================================
        // PITCH INFORMATION (All values in cents unless noted)
        // ============================================================================
        
        // Pitch keycenter - same as "key" above (default: 60)
        // This is the reference note for pitch calculations
        regionData["pitch_keycenter"] = nb::int_(region->pitchKeycenter);
        
        // Pitch tracking per key - how much pitch changes per semitone (default: 100)
        // 100 cents = 1 semitone, 1200 cents = 1 octave
        // Positive values: higher keys = higher pitch
        // Negative values: higher keys = lower pitch (rare)
        regionData["pitch_keytrack"] = nb::float_(region->pitchKeytrack);
    
        // Random pitch variation - adds natural pitch variation to each note (default: 0)
        // Range: 0-12000 cents (0 = no variation, 100 = 1 semitone variation)
        // Creates human-like pitch variation for more natural sound
        regionData["pitch_random"] = nb::float_(region->pitchRandom);

        // Pitch tracking per velocity - how much pitch changes with velocity (default: 0)
        // Positive values: higher velocity = higher pitch
        // Negative values: higher velocity = lower pitch
        regionData["pitch_veltrack"] = nb::float_(region->pitchVeltrack);
        
        // Transpose - pitch shift in semitones (default: 0)
        // Range: typically -12 to +12 semitones
        // Positive values: pitch up, negative values: pitch down
        regionData["transpose"] = nb::float_(region->transpose);
        
        // Fine tuning - pitch adjustment in cents (default: 0)
        // Range: typically -100 to +100 cents
        // Used for fine-tuning the sample's pitch
        regionData["tune"] = nb::float_(region->pitch);
        
        // PITCH CALCULATION RELATIONSHIP (in cents):
        // pitchVariationInCents = pitch_keytrack * (noteNumber - pitch_keycenter)  // note difference
        //                       + tune                                             // sample tuning (regionData['tune'])
        //                       + 100 * transpose                                  // transpose opcode (regionData['transpose'])
        //                       + velocity * pitch_veltrack                        // velocity tracking (regionData['pitch_veltrack'])
        //                       + random(0, pitch_random)                          // random pitch variation (regionData['pitch_random'])
        // Where: noteNumber = MIDI note pressed (0-127)
        
        // ============================================================================
        // SUSTAIN PEDAL INFORMATION
        // ============================================================================
        
        // Whether this region responds to sustain pedal (default: true)
        // true = region respects sustain pedal (CC 64 by default)
        // false = region ignores sustain pedal completely
        regionData["check_sustain"] = nb::bool_(region->checkSustain);
        
        // MIDI CC number for sustain control (default: 64)
        // 64 = standard MIDI sustain pedal
        // Can be set to any CC number (0-127) for custom sustain control
        regionData["sustain_cc"] = nb::int_(region->sustainCC);
        
        // SUSTAIN BEHAVIOR:
        // - When check_sustain=true: Notes continue playing after key release if sustain pedal is pressed
        // - When check_sustain=false: Notes stop immediately when key is released (no pedal effect)
        // - Natural sustain (from sample envelope/loops) works regardless of check_sustain setting
        
        // ============================================================================
        // LOOP INFORMATION
        // ============================================================================
        
        // Loop mode - how the sample plays back (default: "no_loop")
        std::string loopModeStr;
        if (region->loopMode.has_value()) {
            switch (region->loopMode.value()) {
                case sfz::LoopMode::no_loop: loopModeStr = "no_loop"; break;
                case sfz::LoopMode::one_shot: loopModeStr = "one_shot"; break;
                case sfz::LoopMode::loop_continuous: loopModeStr = "loop_continuous"; break;
                case sfz::LoopMode::loop_sustain: loopModeStr = "loop_sustain"; break;
            }
        } else {
            loopModeStr = "no_loop"; // Default when not specified
        }
        regionData["loop_mode"] = nb::str(loopModeStr.c_str());
        
        // LOOP MODE BEHAVIORS:
        // - "no_loop": Sample plays from start to end OR until note-off, whichever comes first
        // - "one_shot": Sample plays from start to end, completely ignoring note-off events (common for drums)
        // - "loop_continuous": Sample loops continuously from loop_start to loop_end
        // - "loop_sustain": Sample loops only while key is held down
        
        // ============================================================================
        // TRIGGER INFORMATION (Mutually Exclusive)
        // ============================================================================
        
        // Trigger type - when this region activates (default: "attack")
        std::string triggerStr;
        switch (region->trigger) {
            case sfz::Trigger::attack: triggerStr = "attack"; break;
            case sfz::Trigger::release: triggerStr = "release"; break;
            case sfz::Trigger::release_key: triggerStr = "release_key"; break;
            case sfz::Trigger::first: triggerStr = "first"; break;
            case sfz::Trigger::legato: triggerStr = "legato"; break;
        }
        regionData["trigger"] = nb::str(triggerStr.c_str());
        
        // TRIGGER BEHAVIORS (Only one can be active per region):
        // - "attack": Triggers when note is pressed (normal playback)
        // - "release": Triggers when note is released AND sustain pedal is pressed
        // - "release_key": Triggers when note is released (regardless of sustain pedal)
        // - "first": Triggers only on the first note when no other notes are playing
        // - "legato": Triggers only on subsequent notes when other notes are already playing
        
        return regionData;
    }

    // Get all region indices that respond to a specific MIDI note
    std::vector<int> getRegionsForNote(int midiNote) const {
        std::vector<int> regionIndices;
        
        auto* synth_handle = synth_.handle();
        if (!synth_handle) {
            throw nb::value_error("No SFZ file loaded. Call load_sfz_file() first.");
        }
        
        // Bounds checking
        if (midiNote < 0 || midiNote > 127) {
            std::string errorMsg = "MIDI note " + std::to_string(midiNote) + 
                                 " out of range. Valid range: 0 to 127";
            throw nb::value_error(errorMsg.c_str());
        }
        
        int numRegions = synth_handle->synth.getNumRegions();
        for (int i = 0; i < numRegions; i++) {
            const auto* region = synth_handle->synth.getRegionView(i);
            if (region && region->keyRange.containsWithEnd(midiNote)) {
                regionIndices.push_back(i);
            }
        }
        
        return regionIndices;
    }
};

NB_MODULE(pysfizz, m) {
    nb::class_<Parser>(m, "Parser")
        .def(nb::init<>())
        .def("load_sfz_file", &Parser::loadSfzFile)
        .def("get_num_regions", &Parser::getNumRegions)
        .def("get_region_data", &Parser::getRegionData)
        .def("get_regions_for_note", &Parser::getRegionsForNote);
}
