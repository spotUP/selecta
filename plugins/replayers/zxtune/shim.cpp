// SPDX-License-Identifier: GPL-3.0-or-later
// ZXTune wrapper for the Selecta zxtune replayer (row I421). The API calls follow bbmp's csrc/zxtune/main.cpp
// (GPL-3.0-or-later, https://github.com/creep-ltx/bbmp); ZXTune itself is LGPL-3.0.
#include "shim.h"

#include "binary/container_factories.h"
#include "core/data_location.h"
#include "core/plugin.h"
#include "core/service.h"
#include "core/plugin_attrs.h"
#include "core/plugins/archive_plugin.h"
#include "core/src/location.h"
#include "module/attributes.h"
#include "parameters/container.h"
#include "sound/chunk.h"

#include <cstring>
#include <string>
#include <vector>

namespace
{
  class Collect : public Module::DetectCallback
  {
  public:
    Parameters::Container::Ptr CreateInitialProperties(StringView) const override
    {
      return Parameters::Container::Create();
    }

    void ProcessModule(const ZXTune::DataLocation&, const ZXTune::Plugin&, Module::Holder::Ptr holder) override
    {
      Tracks.push_back(std::move(holder));
    }

    Log::ProgressCallback* GetProgress() const override
    {
      return nullptr;
    }

    std::vector<Module::Holder::Ptr> Tracks;
  };

  // Probe callback: any module, or any nested data an archive plugin unpacked, counts as recognised. It does not
  // recurse, so the work stays inside the probe window.
  class Recognise : public ZXTune::ArchiveCallback
  {
  public:
    Parameters::Container::Ptr CreateInitialProperties(StringView) const override
    {
      return Parameters::Container::Create();
    }

    void ProcessModule(const ZXTune::DataLocation&, const ZXTune::Plugin&, Module::Holder::Ptr) override
    {
      Hit = true;
    }

    void ProcessUnknownData(const ZXTune::DataLocation&) override {}

    Log::ProgressCallback* GetProgress() const override
    {
      return nullptr;
    }

    void ProcessData(ZXTune::DataLocation::Ptr) override
    {
      Hit = true;
    }

    bool Hit = false;
  };

  std::string Attr(const Parameters::Accessor& props, StringView name)
  {
    Parameters::StringType value;
    Parameters::FindValue(props, Parameters::Identifier(name), value);
    return std::string(value);
  }
}  // namespace

struct zx_shim
{
  Parameters::Container::Ptr Params;
  Collect Found;
  Module::Holder::Ptr Current;
  Module::Renderer::Ptr Renderer;
  Sound::Chunk Pending;
  std::size_t PendingPos = 0;
  std::string Info[5];
  uint32_t DurationMs = 0;
  uint32_t Rate = 0;
};

// Probe (I1078): is `data` a module, or a container holding one, that ZXTune takes at the start of the data?
// OpenModule asks every player plugin to recognise the start of the data; then every archive plugin except the raw
// scanner (Raw::Scaner, which walks the whole buffer byte by byte looking for modules embedded at any offset) gets to
// unpack the start (.trd, .scl, .tap, hrip, ...). Everything is limited to a 1 MiB window, so the cost does not grow
// with the file size. zx_shim_open (playback) keeps DetectModules.
extern "C" int zx_shim_probe(const uint8_t* data, size_t size)
{
  constexpr size_t WINDOW = 1u << 20;
  try
  {
    const auto params = Parameters::Container::Create();
    const auto service = ZXTune::Service::Create(params);
    const auto window = Binary::CreateContainer(Binary::View(data, size < WINDOW ? size : WINDOW));
    try
    {
      if (service->OpenModule(window, StringView(), Parameters::Container::Create()))
      {
        return 1;
      }
    }
    catch (...)
    {
    }
    for (const auto& plugin : ZXTune::ArchivePlugin::Enumerate())
    {
      if ((plugin->Capabilities() & ZXTune::Capabilities::Category::MASK) != ZXTune::Capabilities::Category::CONTAINER
          || (plugin->Capabilities() & ZXTune::Capabilities::Container::Type::MASK)
                 == ZXTune::Capabilities::Container::Type::SCANER)
      {
        continue;
      }
      Recognise found;
      try
      {
        plugin->Detect(*params, ZXTune::CreateLocation(window), found);
      }
      catch (...)
      {
      }
      if (found.Hit)
      {
        return 1;
      }
    }
    return 0;
  }
  catch (...)
  {
    return 0;
  }
}

extern "C" zx_shim* zx_shim_open(const uint8_t* data, size_t size)
{
  try
  {
    auto s = std::make_unique<zx_shim>();
    s->Params = Parameters::Container::Create();
    const auto service = ZXTune::Service::Create(s->Params);
    service->DetectModules(Binary::CreateContainer(Binary::View(data, size)), s->Found);
    if (s->Found.Tracks.empty())
    {
      return nullptr;
    }
    return s.release();
  }
  catch (...)
  {
    return nullptr;
  }
}

extern "C" uint32_t zx_shim_tracks(const zx_shim* s)
{
  return static_cast<uint32_t>(s->Found.Tracks.size());
}

extern "C" int zx_shim_start(zx_shim* s, uint32_t track, uint32_t rate)
{
  try
  {
    if (track >= s->Found.Tracks.size() || rate == 0)
    {
      return 0;
    }
    s->Current = s->Found.Tracks[track];
    const auto props = s->Current->GetModuleProperties();
    s->Info[0] = Attr(*props, Module::ATTR_TITLE);
    s->Info[1] = Attr(*props, Module::ATTR_AUTHOR);
    s->Info[2] = Attr(*props, Module::ATTR_PROGRAM);
    s->Info[3] = Attr(*props, Module::ATTR_TYPE);
    s->Info[4] = Attr(*props, Module::ATTR_COMMENT);
    s->DurationMs = static_cast<uint32_t>(s->Current->GetModuleInformation().Duration.Get());
    s->Renderer = s->Current->CreateRenderer(rate, props);
    s->Rate = rate;
    s->Pending = Sound::Chunk();
    s->PendingPos = 0;
    return 1;
  }
  catch (...)
  {
    return 0;
  }
}

extern "C" const char* zx_shim_info(const zx_shim* s, uint32_t index)
{
  return index < 5 ? s->Info[index].c_str() : "";
}

extern "C" uint32_t zx_shim_duration_ms(const zx_shim* s)
{
  return s->DurationMs;
}

extern "C" uint32_t zx_shim_render(zx_shim* s, int16_t* out, uint32_t frames)
{
  try
  {
    uint32_t done = 0;
    while (done < frames)
    {
      if (s->PendingPos >= s->Pending.size())
      {
        s->Pending = s->Renderer->Render();
        s->PendingPos = 0;
        if (s->Pending.empty())
        {
          break;
        }
      }
      const std::size_t n = std::min<std::size_t>(frames - done, s->Pending.size() - s->PendingPos);
      // Sound::Sample packs left in the low 16 bits and right in the high 16 bits: s16le stereo
      std::memcpy(out + 2 * done, s->Pending.data() + s->PendingPos, n * sizeof(Sound::Sample));
      s->PendingPos += n;
      done += static_cast<uint32_t>(n);
    }
    return done;
  }
  catch (...)
  {
    return 0;
  }
}

extern "C" int zx_shim_seek(zx_shim* s, uint32_t ms)
{
  try
  {
    s->Renderer->SetPosition(Time::AtMillisecond(ms));
    s->Pending = Sound::Chunk();
    s->PendingPos = 0;
    return 1;
  }
  catch (...)
  {
    return 0;
  }
}

extern "C" void zx_shim_close(zx_shim* s)
{
  delete s;
}
