#pragma once

#include <stdint.h>
#include "../data/isotope_db.h"

// Learned ambient level of the natural decay chains, per room.
//
// Why this exists
// ---------------
// Every room contains the U-238 and Th-232 chains: radon daughters in the air
// and thorium in concrete, brick and stone. So a significant fitted chain
// amplitude does NOT by itself mean the SAMPLE contains one - the room alone
// produces several sigma of it on a long run, because the fit's significance
// is a statistical measure and ambient counts are real counts.
//
// The previous answer was a fixed floor: the chain had to account for >= 4% of
// all counts. That number was read off the two available captures (ambient
// gives U 0.8% / Th 1.9%, the ore 7.4%), and it worked for those. It fails for
// anything in between, and it fails in a specific and misleading way: a real
// uranium ore measured at 0.18 uSv/h reaches 8 sigma - an overwhelming
// detection - and is then discarded for sitting at 3.7% instead of 4%. Because
// that cutoff lands almost exactly at 0.20 uSv/h for this ore, the device
// looked like it had a hardcoded dose threshold, and a sample hovering either
// side of it flickered between a verdict and none.
//
// What replaces it
// ----------------
// Ambient is measured rather than assumed. The chain fraction of a room is
// stable over hours, so the floor of what this detector sees in this room IS
// the ambient level - and a sample is elevated when it stands clear of that
// floor, by a ratio rather than by an absolute share of the spectrum.
//
// Two properties make the ratio the right comparison:
//
//  * It is scale free. A sample that doubles the chain relative to the room is
//    a finding whether the room is a wooden shed or a granite cellar, and no
//    constant has to be re-tuned for either.
//  * It resists the residual cross-talk that pure significance cannot. The
//    response model is imperfect, so the uranium ore leaks ~2.5% into the
//    thorium chain; that leak is a bias, so its SIGNIFICANCE grows without
//    limit as counting continues while its FRACTION stays put. Testing the
//    fraction therefore keeps working on long runs, where a sigma cutoff would
//    eventually declare the cross-talk real.
//
// With the floor learned, the binding constraint becomes counting statistics -
// DETECT_SIGMA and time - which is what the user can actually act on by
// measuring for longer. That is the whole point of the change.
namespace Ambient {

  // Loads the learned levels from NVS. Safe to call before any measurement.
  void begin();

  // The current ambient estimate for a chain, as a fraction of total counts.
  float fractionOf(DecayChain c);

  // True when `frac` stands clear of the learned ambient level for `c`.
  // Significance is gated separately by the caller (DETECT_SIGMA); this
  // answers only "is this more chain than the room supplies".
  bool elevated(DecayChain c, float frac);

  // Feeds one analysis result back into the estimate.
  //
  // `identified` says whether the analysis is currently naming a source. The
  // estimate is allowed to fall freely - ambient is a floor, and a lower
  // reading is always better evidence of where the floor is - but may only
  // rise while nothing is identified, so a sample left on the detector for
  // hours cannot slowly teach the device to ignore it.
  void observe(DecayChain c, float frac, uint32_t windowSeconds,
               double totalCounts, bool identified);

  // Forgets the learned levels and returns to the seeded defaults. For a
  // factory reset, or after moving the detector somewhere very different.
  void reset();

  // True once a level has been learned here rather than merely seeded, so the
  // UI can distinguish "measured in this room" from "assumed".
  bool learned(DecayChain c);
}
