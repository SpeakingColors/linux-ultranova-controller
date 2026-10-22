// A software model of how the UltraNova receives CC and NRPN, written from the text of Novation's MIDI assignments
// (nrpnMSB is 0 at power-up and after every data entry; 14 bit values apply when dataLSB arrives; later dataLSBs are
// ignored; other controllers do not disturb a running sequence). It is NOT a recording of the real device: it checks
// that our messages follow the documented rules. It shares no code with src/MidiCore.h on purpose.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

struct SimUltraNova
{
    int cc[128]; int nrpn[128][128]; bool nrpnSet[128][128]; bool ccSet[128];
    int msb = 0, lsb = -1, hi = 0; bool wait14 = false; int wm = 0, wl = 0;   // pending 14 bit address
    int msgs = 0, bad = 0;
    SimUltraNova() { std::memset(cc, 0, sizeof cc); std::memset(nrpn, 0, sizeof nrpn); std::memset(nrpnSet, 0, sizeof nrpnSet); std::memset(ccSet, 0, sizeof ccSet); }
    static bool is14(int m, int l) { return m == 2 && l == 63; }   // ClockBPM, the only 14 bit NRPN listed
    void receive(uint8_t status, uint8_t n, uint8_t v)
    {
        ++msgs;
        if ((status & 0xF0) != 0xB0 || n > 127 || v > 127) { ++bad; return; }
        if (n == 99) { msb = v; lsb = -1; wait14 = false; return; }
        if (n == 98) { lsb = v; wait14 = false; return; }
        if (n == 6)
        {
            const int m = msb; msb = 0;
            if (lsb < 0) return;
            if (is14(m, lsb)) { wait14 = true; wm = m; wl = lsb; hi = v; return; }
            wait14 = false; nrpn[m][lsb] = v; nrpnSet[m][lsb] = true; return;
        }
        if (n == 38) { if (wait14) { nrpn[wm][wl] = hi << 7 | v; nrpnSet[wm][wl] = true; wait14 = false; } return; }
        cc[n] = v; ccSet[n] = true;
    }
};
