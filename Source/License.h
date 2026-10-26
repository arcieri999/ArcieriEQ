#pragma once
#include <juce_core/juce_core.h>

// Sistema de licencias de Arcieri EQ.
// Aqui solo vive la clave PUBLICA (sirve para verificar, no para crear licencias).
namespace License
{
    inline const char* exponentHex() { return "10001"; }
    inline const char* modulusHex()  { return "ad3388e3a44c91ab3360858f580cbeac3a89fa67c58b46094cfa32eb1d8a1e97b4b04b1bf4d50121f8a9ba4755aa8c4e119bb796dee37b84a385d2fddd8aabf1c3e0a732b103b530c7ac0efd96340be54ef91346a069488b63fdc98b4ddc5dc596113b67e7ec9127bbf389978e95ee188783619c321d5b3454dcc8c308f35e95"; }

    // Codigo de este equipo, formato XXXX-XXXX-XXXX-XXXX
    inline juce::String machineCode()
    {
        juce::String id = juce::SystemStats::getUniqueDeviceID();
        if (id.isEmpty())
            id = juce::SystemStats::getComputerName() + juce::SystemStats::getLogonName();

        juce::String s = "ARCIERI-MACHINE|" + id;
        juce::SHA256 h (s.toRawUTF8(), s.getNumBytesAsUTF8());
        auto hex = h.toHexString().substring (0, 16).toUpperCase();
        return hex.substring (0, 4) + "-" + hex.substring (4, 8) + "-" + hex.substring (8, 12) + "-" + hex.substring (12, 16);
    }

    inline bool verify (const juce::String& machineCodeNormalised, juce::String keyHex)
    {
        keyHex = keyHex.retainCharacters ("0123456789abcdefABCDEF").toLowerCase();
        if (keyHex.length() < 64) return false;

        juce::BigInteger sig, e, n, expected;
        sig.parseString (keyHex, 16);
        e.parseString (exponentHex(), 16);
        n.parseString (modulusHex(), 16);
        if (sig >= n) return false;

        sig.exponentModulo (e, n);

        juce::String msg = "ARCIERI-EQ|" + machineCodeNormalised;
        juce::SHA256 h (msg.toRawUTF8(), msg.getNumBytesAsUTF8());
        expected.parseString (h.toHexString(), 16);
        return sig == expected;
    }

    inline juce::File licenseFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Arcieri").getChildFile ("ArcieriEQ.lic");
    }

    inline bool isLicensed()
    {
        auto f = licenseFile();
        if (! f.existsAsFile()) return false;
        return verify (machineCode().removeCharacters ("-"), f.loadFileAsString());
    }

    inline bool activate (const juce::String& key)
    {
        if (! verify (machineCode().removeCharacters ("-"), key)) return false;
        auto f = licenseFile();
        f.getParentDirectory().createDirectory();
        return f.replaceWithText (key.trim());
    }
}
