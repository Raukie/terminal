// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Appearances.h"
#include "Appearances.g.cpp"
#include "AxisKeyValuePair.g.cpp"
#include "FeatureKeyValuePair.g.cpp"

#include "EnumEntry.h"
#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static constexpr std::array<std::wstring_view, 11> DefaultFeatures{
    L"rlig",
    L"locl",
    L"ccmp",
    L"calt",
    L"liga",
    L"clig",
    L"rnrn",
    L"kern",
    L"mark",
    L"mkmk",
    L"dist"
};

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring> Font::FontAxesTagsAndNames()
    {
        if (!_fontAxesTagsAndNames)
        {
            wil::com_ptr<IDWriteFont> font;
            THROW_IF_FAILED(_family->GetFont(0, font.put()));
            wil::com_ptr<IDWriteFontFace> fontFace;
            THROW_IF_FAILED(font->CreateFontFace(fontFace.put()));
            wil::com_ptr<IDWriteFontFace5> fontFace5;
            if (fontFace5 = fontFace.try_query<IDWriteFontFace5>())
            {
                wil::com_ptr<IDWriteFontResource> fontResource;
                THROW_IF_FAILED(fontFace5->GetFontResource(fontResource.put()));

                const auto axesCount = fontFace5->GetFontAxisValueCount();
                if (axesCount > 0)
                {
                    std::vector<DWRITE_FONT_AXIS_VALUE> axesVector(axesCount);
                    fontFace5->GetFontAxisValues(axesVector.data(), axesCount);

                    uint32_t localeIndex;
                    BOOL localeExists;
                    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
                    const auto localeToTry = GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) ? localeName : L"en-US";

                    std::unordered_map<winrt::hstring, winrt::hstring> fontAxesTagsAndNames;
                    for (uint32_t i = 0; i < axesCount; ++i)
                    {
                        wil::com_ptr<IDWriteLocalizedStrings> names;
                        THROW_IF_FAILED(fontResource->GetAxisNames(i, names.put()));

                        if (!SUCCEEDED(names->FindLocaleName(localeToTry, &localeIndex, &localeExists)) || !localeExists)
                        {
                            // default to the first locale in the list
                            localeIndex = 0;
                        }

                        UINT32 length = 0;
                        if (SUCCEEDED(names->GetStringLength(localeIndex, &length)))
                        {
                            winrt::impl::hstring_builder builder{ length };
                            if (SUCCEEDED(names->GetString(localeIndex, builder.data(), length + 1)))
                            {
                                fontAxesTagsAndNames.insert(std::pair<winrt::hstring, winrt::hstring>(_tagToString(axesVector[i].axisTag), builder.to_hstring()));
                                continue;
                            }
                        }
                        // if there was no name found, it means the font does not actually support this axis
                        // don't insert anything into the vector in this case
                    }
                    _fontAxesTagsAndNames = winrt::single_threaded_map(std::move(fontAxesTagsAndNames));
                }
            }
        }
        return _fontAxesTagsAndNames;
    }

    IMap<hstring, hstring> Font::FontFeaturesTagsAndNames()
    {
        if (!_fontFeaturesTagsAndNames)
        {
            wil::com_ptr<IDWriteFont> font;
            THROW_IF_FAILED(_family->GetFont(0, font.put()));
            wil::com_ptr<IDWriteFontFace> fontFace;
            THROW_IF_FAILED(font->CreateFontFace(fontFace.put()));

            wil::com_ptr<IDWriteFactory2> factory;
            THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory), reinterpret_cast<::IUnknown**>(factory.addressof())));
            wil::com_ptr<IDWriteTextAnalyzer> textAnalyzer;
            factory->CreateTextAnalyzer(textAnalyzer.addressof());
            wil::com_ptr<IDWriteTextAnalyzer2> textAnalyzer2 = textAnalyzer.query<IDWriteTextAnalyzer2>();

            DWRITE_SCRIPT_ANALYSIS scriptAnalysis{};
            UINT32 tagCount;
            // we have to call GetTypographicFeatures twice, first to get the actual count then to get the features
            std::ignore = textAnalyzer2->GetTypographicFeatures(fontFace.get(), scriptAnalysis, L"en-us", 0, &tagCount, nullptr);
            std::vector<DWRITE_FONT_FEATURE_TAG> tags{ tagCount };
            textAnalyzer2->GetTypographicFeatures(fontFace.get(), scriptAnalysis, L"en-us", tagCount, &tagCount, tags.data());

            std::unordered_map<winrt::hstring, winrt::hstring> fontFeaturesTagsAndNames;
            for (auto tag : tags)
            {
                const auto tagString = _tagToString(tag);
                hstring formattedResourceString{ fmt::format(L"Profile_FontFeature_{}", tagString) };
                hstring localizedName{ tagString };
                // we have resource strings for common font features, see if one for this feature exists
                if (HasLibraryResourceWithName(formattedResourceString))
                {
                    localizedName = GetLibraryResourceString(formattedResourceString);
                }
                fontFeaturesTagsAndNames.insert(std::pair<winrt::hstring, winrt::hstring>(tagString, localizedName));
            }
            _fontFeaturesTagsAndNames = winrt::single_threaded_map<winrt::hstring, winrt::hstring>(std::move(fontFeaturesTagsAndNames));
        }
        return _fontFeaturesTagsAndNames;
    }

    winrt::hstring Font::_tagToString(DWRITE_FONT_AXIS_TAG tag)
    {
        std::wstring result;
        for (int i = 0; i < 4; ++i)
        {
            result.push_back((tag >> (i * 8)) & 0xFF);
        }
        return winrt::hstring{ result };
    }

    hstring Font::_tagToString(DWRITE_FONT_FEATURE_TAG tag)
    {
        std::wstring result;
        for (int i = 0; i < 4; ++i)
        {
            result.push_back((tag >> (i * 8)) & 0xFF);
        }
        return hstring{ result };
    }

    AxisKeyValuePair::AxisKeyValuePair(winrt::hstring axisKey, float axisValue, const Windows::Foundation::Collections::IMap<winrt::hstring, float>& baseMap, const Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>& tagToNameMap) :
        _AxisKey{ axisKey },
        _AxisValue{ axisValue },
        _baseMap{ baseMap },
        _tagToNameMap{ tagToNameMap }
    {
        if (_tagToNameMap.HasKey(_AxisKey))
        {
            int32_t i{ 0 };
            // IMap guarantees that the iteration order is the same every time
            // so this conversion of key to index is safe
            for (const auto tagAndName : _tagToNameMap)
            {
                if (tagAndName.Key() == _AxisKey)
                {
                    _AxisIndex = i;
                    break;
                }
                ++i;
            }
        }
    }

    winrt::hstring AxisKeyValuePair::AxisKey()
    {
        return _AxisKey;
    }

    float AxisKeyValuePair::AxisValue()
    {
        return _AxisValue;
    }

    int32_t AxisKeyValuePair::AxisIndex()
    {
        return _AxisIndex;
    }

    void AxisKeyValuePair::AxisValue(float axisValue)
    {
        if (axisValue != _AxisValue)
        {
            _baseMap.Remove(_AxisKey);
            _AxisValue = axisValue;
            _baseMap.Insert(_AxisKey, _AxisValue);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"AxisValue" });
        }
    }

    void AxisKeyValuePair::AxisKey(winrt::hstring axisKey)
    {
        if (axisKey != _AxisKey)
        {
            _baseMap.Remove(_AxisKey);
            _AxisKey = axisKey;
            _baseMap.Insert(_AxisKey, _AxisValue);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"AxisKey" });
        }
    }

    void AxisKeyValuePair::AxisIndex(int32_t axisIndex)
    {
        if (axisIndex != _AxisIndex)
        {
            _AxisIndex = axisIndex;

            int32_t i{ 0 };
            // same as in the constructor, iterating through IMap
            // gives us the same order every time
            for (const auto tagAndName : _tagToNameMap)
            {
                if (i == _AxisIndex)
                {
                    AxisKey(tagAndName.Key());
                    break;
                }
                ++i;
            }
        }
    }

    FeatureKeyValuePair::FeatureKeyValuePair(hstring featureKey, uint32_t featureValue, const IMap<hstring, uint32_t>& baseMap, const IMap<hstring, hstring>& tagToNameMap) :
        _FeatureKey{ featureKey },
        _FeatureValue{ featureValue },
        _baseMap{ baseMap },
        _tagToNameMap{ tagToNameMap }
    {
        if (_tagToNameMap.HasKey(_FeatureKey))
        {
            int32_t i{ 0 };
            // this loop assumes that every time we iterate through the map
            // we get the same ordering
            for (const auto tagAndName : _tagToNameMap)
            {
                if (tagAndName.Key() == _FeatureKey)
                {
                    _FeatureIndex = i;
                    break;
                }
                ++i;
            }
        }
    }

    hstring FeatureKeyValuePair::FeatureKey()
    {
        return _FeatureKey;
    }

    uint32_t FeatureKeyValuePair::FeatureValue()
    {
        return _FeatureValue;
    }

    int32_t FeatureKeyValuePair::FeatureIndex()
    {
        return _FeatureIndex;
    }

    void FeatureKeyValuePair::FeatureValue(uint32_t featureValue)
    {
        if (featureValue != _FeatureValue)
        {
            _baseMap.Remove(_FeatureKey);
            _FeatureValue = featureValue;
            _baseMap.Insert(_FeatureKey, _FeatureValue);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"FeatureValue" });
        }
    }

    void FeatureKeyValuePair::FeatureKey(hstring featureKey)
    {
        if (featureKey != _FeatureKey)
        {
            _baseMap.Remove(_FeatureKey);
            _FeatureKey = featureKey;
            _baseMap.Insert(_FeatureKey, _FeatureValue);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"FeatureKey" });
        }
    }

    void FeatureKeyValuePair::FeatureIndex(int32_t featureIndex)
    {
        if (featureIndex != _FeatureIndex)
        {
            _FeatureIndex = featureIndex;

            int32_t i{ 0 };
            // same as in the constructor, this assumes that iterating through the map
            // gives us the same order every time
            for (const auto tagAndName : _tagToNameMap)
            {
                if (i == _FeatureIndex)
                {
                    FeatureKey(tagAndName.Key());
                    break;
                }
                ++i;
            }
        }
    }

    AppearanceViewModel::AppearanceViewModel(const Model::AppearanceConfig& appearance) :
        _appearance{ appearance }
    {
        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        //  unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"BackgroundImagePath")
            {
                // notify listener that all background image related values might have changed
                //
                // We need to do this so if someone manually types "desktopWallpaper"
                // into the path TextBox, we properly update the checkbox and stored
                // _lastBgImagePath. Without this, then we'll permanently hide the text
                // box, prevent it from ever being changed again.
                _NotifyChanges(L"UseDesktopBGImage", L"BackgroundImageSettingsVisible");
            }
            else if (viewModelProperty == L"FontAxes")
            {
                // this is a weird one
                // we manually make the observable vector based on the map in the settings model
                // (this is due to xaml being unable to bind a list view to a map)
                // so when the FontAxes change (say from the reset button), reinitialize the observable vector
                InitializeFontAxesVector();
            }
            else if (viewModelProperty == L"FontFeatures")
            {
                // same as the FontAxes one
                InitializeFontFeaturesVector();
            }
        });

        _refreshFontFaceDependents();
        InitializeFontAxesVector();
        InitializeFontFeaturesVector();

        // Cache the original BG image path. If the user clicks "Use desktop
        // wallpaper", then un-checks it, this is the string we'll restore to
        // them.
        if (BackgroundImagePath() != L"desktopWallpaper")
        {
            _lastBgImagePath = BackgroundImagePath();
        }
    }

    winrt::hstring AppearanceViewModel::FontFace() const
    {
        return _appearance.SourceProfile().FontInfo().FontFace();
    }

    void AppearanceViewModel::FontFace(const winrt::hstring& value)
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        if (fontInfo.FontFace() == value)
        {
            return;
        }

        fontInfo.FontFace(value);
        _refreshFontFaceDependents();

        _NotifyChanges(L"HasFontFace", L"FontFace");
    }

    bool AppearanceViewModel::HasFontFace() const
    {
        return _appearance.SourceProfile().FontInfo().HasFontFace();
    }

    void AppearanceViewModel::ClearFontFace()
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        const auto hadValue = fontInfo.HasFontFace();

        fontInfo.ClearFontFace();
        _refreshFontFaceDependents();

        if (hadValue)
        {
            _NotifyChanges(L"HasFontFace", L"FontFace");
        }
    }

    Model::FontConfig AppearanceViewModel::FontFaceOverrideSource() const
    {
        return _appearance.SourceProfile().FontInfo().FontFaceOverrideSource();
    }

    void AppearanceViewModel::_refreshFontFaceDependents()
    {
        wil::com_ptr<IDWriteFactory> factory;
        THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(factory), reinterpret_cast<::IUnknown**>(factory.addressof())));

        wil::com_ptr<IDWriteFontCollection> fontCollection;
        THROW_IF_FAILED(factory->GetSystemFontCollection(fontCollection.addressof(), FALSE));

        const auto fontFace = FontFace();
        std::wstring primaryFontName;
        std::wstring missingFonts;
        std::wstring proportionalFonts;
        BOOL hasPowerlineCharacters = FALSE;

        til::iterate_font_families(fontFace, [&](wil::zwstring_view name) {
            if (primaryFontName.empty())
            {
                primaryFontName = name;
            }

            std::wstring* accumulator = nullptr;

            UINT32 index = 0;
            BOOL exists = FALSE;
            THROW_IF_FAILED(fontCollection->FindFamilyName(name.c_str(), &index, &exists));

            // Look ma, no goto!
            do
            {
                if (!exists)
                {
                    accumulator = &missingFonts;
                    break;
                }

                wil::com_ptr<IDWriteFontFamily> fontFamily;
                THROW_IF_FAILED(fontCollection->GetFontFamily(index, fontFamily.addressof()));

                wil::com_ptr<IDWriteFont> font;
                THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.addressof()));

                if (!font.query<IDWriteFont1>()->IsMonospacedFont())
                {
                    accumulator = &proportionalFonts;
                }

                // We're actually checking for the "Extended" PowerLine glyph set.
                // They're more fun.
                BOOL hasE0B6 = FALSE;
                std::ignore = font->HasCharacter(0xE0B6, &hasE0B6);
                hasPowerlineCharacters |= hasE0B6;
            } while (false);

            if (accumulator)
            {
                if (!accumulator->empty())
                {
                    accumulator->append(L", ");
                }
                accumulator->append(name);
            }
        });

        _primaryFontName = std::move(primaryFontName);
        MissingFontFaces(winrt::hstring{ missingFonts });
        ProportionalFontFaces(winrt::hstring{ proportionalFonts });
        HasPowerlineCharacters(hasPowerlineCharacters);
    }

    double AppearanceViewModel::LineHeight() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        const auto cellHeight = fontInfo.CellHeight();
        const auto str = cellHeight.c_str();

        auto& errnoRef = errno; // Nonzero cost, pay it once.
        errnoRef = 0;

        wchar_t* end;
        const auto value = std::wcstod(str, &end);

        return str == end || errnoRef == ERANGE ? NAN : value;
    }

    void AppearanceViewModel::LineHeight(const double value)
    {
        std::wstring str;

        if (value >= 0.1 && value <= 10.0)
        {
            str = fmt::format(FMT_STRING(L"{:.6g}"), value);
        }

        const auto fontInfo = _appearance.SourceProfile().FontInfo();

        if (fontInfo.CellHeight() != str)
        {
            if (str.empty())
            {
                fontInfo.ClearCellHeight();
            }
            else
            {
                fontInfo.CellHeight(str);
            }
            _NotifyChanges(L"HasLineHeight", L"LineHeight");
        }
    }

    bool AppearanceViewModel::HasLineHeight() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.HasCellHeight();
    }

    void AppearanceViewModel::ClearLineHeight()
    {
        LineHeight(NAN);
    }

    Model::FontConfig AppearanceViewModel::LineHeightOverrideSource() const
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        return fontInfo.CellHeightOverrideSource();
    }

    void AppearanceViewModel::SetFontWeightFromDouble(double fontWeight)
    {
        FontWeight(winrt::Microsoft::Terminal::UI::Converters::DoubleToFontWeight(fontWeight));
    }

    void AppearanceViewModel::SetBackgroundImageOpacityFromPercentageValue(double percentageValue)
    {
        BackgroundImageOpacity(winrt::Microsoft::Terminal::UI::Converters::PercentageValueToPercentage(percentageValue));
    }

    void AppearanceViewModel::SetBackgroundImagePath(winrt::hstring path)
    {
        BackgroundImagePath(path);
    }

    bool AppearanceViewModel::UseDesktopBGImage()
    {
        return BackgroundImagePath() == L"desktopWallpaper";
    }

    void AppearanceViewModel::UseDesktopBGImage(const bool useDesktop)
    {
        if (useDesktop)
        {
            // Stash the current value of BackgroundImagePath. If the user
            // checks and un-checks the "Use desktop wallpaper" button, we want
            // the path that we display in the text box to remain unchanged.
            //
            // Only stash this value if it's not the special "desktopWallpaper"
            // value.
            if (BackgroundImagePath() != L"desktopWallpaper")
            {
                _lastBgImagePath = BackgroundImagePath();
            }
            BackgroundImagePath(L"desktopWallpaper");
        }
        else
        {
            // Restore the path we had previously cached. This might be the
            // empty string.
            BackgroundImagePath(_lastBgImagePath);
        }
    }

    bool AppearanceViewModel::BackgroundImageSettingsVisible()
    {
        return BackgroundImagePath() != L"";
    }

    void AppearanceViewModel::ClearColorScheme()
    {
        ClearDarkColorSchemeName();
        _NotifyChanges(L"CurrentColorScheme");
    }

    Editor::ColorSchemeViewModel AppearanceViewModel::CurrentColorScheme()
    {
        const auto schemeName{ DarkColorSchemeName() };
        const auto allSchemes{ SchemesList() };
        for (const auto& scheme : allSchemes)
        {
            if (scheme.Name() == schemeName)
            {
                return scheme;
            }
        }
        // This Appearance points to a color scheme that was renamed or deleted.
        // Fallback to the first one in the list.
        return allSchemes.GetAt(0);
    }

    void AppearanceViewModel::CurrentColorScheme(const ColorSchemeViewModel& val)
    {
        DarkColorSchemeName(val.Name());
        LightColorSchemeName(val.Name());
    }

    void AppearanceViewModel::AddNewAxisKeyValuePair()
    {
        if (!_appearance.SourceProfile().FontInfo().FontAxes())
        {
            _appearance.SourceProfile().FontInfo().FontAxes(winrt::single_threaded_map<winrt::hstring, float>());
        }
        auto fontAxesMap = _appearance.SourceProfile().FontInfo().FontAxes();

        // find one axis that does not already exist, and add that
        // if there are no more possible axes to add, the button is disabled so there shouldn't be a way to get here
        const auto possibleAxesTagsAndNames = ProfileViewModel::FindFontWithLocalizedName(_primaryFontName).FontAxesTagsAndNames();
        for (const auto tagAndName : possibleAxesTagsAndNames)
        {
            if (!fontAxesMap.HasKey(tagAndName.Key()))
            {
                fontAxesMap.Insert(tagAndName.Key(), gsl::narrow<float>(0));
                FontAxesVector().Append(_CreateAxisKeyValuePairHelper(tagAndName.Key(), gsl::narrow<float>(0), fontAxesMap, possibleAxesTagsAndNames));
                break;
            }
        }
        _NotifyChanges(L"CanFontAxesBeAdded");
    }

    void AppearanceViewModel::DeleteAxisKeyValuePair(winrt::hstring key)
    {
        for (uint32_t i = 0; i < _FontAxesVector.Size(); i++)
        {
            if (_FontAxesVector.GetAt(i).AxisKey() == key)
            {
                FontAxesVector().RemoveAt(i);
                _appearance.SourceProfile().FontInfo().FontAxes().Remove(key);
                if (_FontAxesVector.Size() == 0)
                {
                    _appearance.SourceProfile().FontInfo().ClearFontAxes();
                }
                break;
            }
        }
        _NotifyChanges(L"CanFontAxesBeAdded");
    }

    void AppearanceViewModel::InitializeFontAxesVector()
    {
        if (!_FontAxesVector)
        {
            _FontAxesVector = winrt::single_threaded_observable_vector<Editor::AxisKeyValuePair>();
        }

        _FontAxesVector.Clear();
        if (const auto fontAxesMap = _appearance.SourceProfile().FontInfo().FontAxes())
        {
            const auto fontAxesTagToNameMap = ProfileViewModel::FindFontWithLocalizedName(_primaryFontName).FontAxesTagsAndNames();
            for (const auto axis : fontAxesMap)
            {
                // only show the axes that the font supports
                // any axes that the font doesn't support continue to be stored in the json, we just don't show them in the UI
                if (fontAxesTagToNameMap.HasKey(axis.Key()))
                {
                    _FontAxesVector.Append(_CreateAxisKeyValuePairHelper(axis.Key(), axis.Value(), fontAxesMap, fontAxesTagToNameMap));
                }
            }
        }
        _NotifyChanges(L"AreFontAxesAvailable", L"CanFontAxesBeAdded");
    }

    // Method Description:
    // - Determines whether the currently selected font has any variable font axes
    bool AppearanceViewModel::AreFontAxesAvailable()
    {
        return ProfileViewModel::FindFontWithLocalizedName(_primaryFontName).FontAxesTagsAndNames().Size() > 0;
    }

    // Method Description:
    // - Determines whether the currently selected font has any variable font axes that have not already been set
    bool AppearanceViewModel::CanFontAxesBeAdded()
    {
        if (const auto fontAxesTagToNameMap = ProfileViewModel::FindFontWithLocalizedName(_primaryFontName).FontAxesTagsAndNames(); fontAxesTagToNameMap.Size() > 0)
        {
            if (const auto fontAxesMap = _appearance.SourceProfile().FontInfo().FontAxes())
            {
                for (const auto tagAndName : fontAxesTagToNameMap)
                {
                    if (!fontAxesMap.HasKey(tagAndName.Key()))
                    {
                        // we found an axis that has not been set
                        return true;
                    }
                }
                // all possible axes have been set already
                return false;
            }
            // the font supports font axes but the profile has none set
            return true;
        }
        // the font does not support any font axes
        return false;
    }

    // Method Description:
    // - Creates an AxisKeyValuePair and sets up an event handler for it
    Editor::AxisKeyValuePair AppearanceViewModel::_CreateAxisKeyValuePairHelper(winrt::hstring axisKey, float axisValue, const Windows::Foundation::Collections::IMap<winrt::hstring, float>& baseMap, const Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>& tagToNameMap)
    {
        const auto axisKeyValuePair = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::AxisKeyValuePair>(axisKey, axisValue, baseMap, tagToNameMap);
        // when either the key or the value changes, send an event for the preview control to catch
        axisKeyValuePair.PropertyChanged([weakThis = get_weak()](auto& /*sender*/, auto& /*e*/) {
            if (auto appVM{ weakThis.get() })
            {
                appVM->_NotifyChanges(L"AxisKeyValuePair");
            }
        });
        return axisKeyValuePair;
    }

    void AppearanceViewModel::AddNewFeatureKeyValuePair()
    {
        const auto fontInfo = _appearance.SourceProfile().FontInfo();
        auto fontFeaturesMap = fontInfo.FontFeatures();
        if (!fontFeaturesMap)
        {
            fontFeaturesMap = winrt::single_threaded_map<hstring, uint32_t>();
            fontInfo.FontFeatures(fontFeaturesMap);
        }

        // find one feature that does not already exist, and add that
        // if there are no more possible features to add, the button is disabled so there shouldn't be a way to get here
        const auto possibleFeaturesTagsAndNames = ProfileViewModel::FindFontWithLocalizedName(_primaryFontName).FontFeaturesTagsAndNames();
        for (const auto tagAndName : possibleFeaturesTagsAndNames)
        {
            const auto featureKey = tagAndName.Key();
            if (!fontFeaturesMap.HasKey(featureKey))
            {
                const auto featureDefaultValue = _IsDefaultFeature(featureKey) ? 1 : 0;
                fontFeaturesMap.Insert(featureKey, featureDefaultValue);
                FontFeaturesVector().Append(_CreateFeatureKeyValuePairHelper(featureKey, featureDefaultValue, fontFeaturesMap, possibleFeaturesTagsAndNames));
                break;
            }
        }
        _NotifyChanges(L"CanFontFeaturesBeAdded");
    }

    void AppearanceViewModel::DeleteFeatureKeyValuePair(hstring key)
    {
        for (uint32_t i = 0; i < _FontFeaturesVector.Size(); i++)
        {
            if (_FontFeaturesVector.GetAt(i).FeatureKey() == key)
            {
                FontFeaturesVector().RemoveAt(i);
                _appearance.SourceProfile().FontInfo().FontFeatures().Remove(key);
                if (_FontFeaturesVector.Size() == 0)
                {
                    _appearance.SourceProfile().FontInfo().ClearFontFeatures();
                }
                break;
            }
        }
        _NotifyChanges(L"CanFontAxesBeAdded");
    }

    void AppearanceViewModel::InitializeFontFeaturesVector()
    {
        if (!_FontFeaturesVector)
        {
            _FontFeaturesVector = single_threaded_observable_vector<Editor::FeatureKeyValuePair>();
        }

        _FontFeaturesVector.Clear();
        if (const auto fontFeaturesMap = _appearance.SourceProfile().FontInfo().FontFeatures())
        {
            const auto fontFeaturesTagToNameMap = ProfileViewModel::FindFontWithLocalizedName(_primaryFontName).FontFeaturesTagsAndNames();
            for (const auto feature : fontFeaturesMap)
            {
                const auto featureKey = feature.Key();
                // only show the features that the font supports
                // any features that the font doesn't support continue to be stored in the json, we just don't show them in the UI
                if (fontFeaturesTagToNameMap.HasKey(featureKey))
                {
                    _FontFeaturesVector.Append(_CreateFeatureKeyValuePairHelper(featureKey, feature.Value(), fontFeaturesMap, fontFeaturesTagToNameMap));
                }
            }
        }
        _NotifyChanges(L"AreFontFeaturesAvailable", L"CanFontFeaturesBeAdded");
    }

    // Method Description:
    // - Determines whether the currently selected font has any font features
    bool AppearanceViewModel::AreFontFeaturesAvailable()
    {
        return ProfileViewModel::FindFontWithLocalizedName(_primaryFontName).FontFeaturesTagsAndNames().Size() > 0;
    }

    // Method Description:
    // - Determines whether the currently selected font has any font features that have not already been set
    bool AppearanceViewModel::CanFontFeaturesBeAdded()
    {
        if (const auto fontFeaturesTagToNameMap = ProfileViewModel::FindFontWithLocalizedName(_primaryFontName).FontFeaturesTagsAndNames(); fontFeaturesTagToNameMap.Size() > 0)
        {
            if (const auto fontFeaturesMap = _appearance.SourceProfile().FontInfo().FontFeatures())
            {
                for (const auto tagAndName : fontFeaturesTagToNameMap)
                {
                    if (!fontFeaturesMap.HasKey(tagAndName.Key()))
                    {
                        // we found a feature that has not been set
                        return true;
                    }
                }
                // all possible features have been set already
                return false;
            }
            // the font supports font features but the profile has none set
            return true;
        }
        // the font does not support any font features
        return false;
    }

    // Method Description:
    // - Creates a FeatureKeyValuePair and sets up an event handler for it
    Editor::FeatureKeyValuePair AppearanceViewModel::_CreateFeatureKeyValuePairHelper(hstring featureKey, uint32_t featureValue, const IMap<hstring, uint32_t>& baseMap, const IMap<hstring, hstring>& tagToNameMap)
    {
        const auto featureKeyValuePair = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::FeatureKeyValuePair>(featureKey, featureValue, baseMap, tagToNameMap);
        // when either the key or the value changes, send an event for the preview control to catch
        featureKeyValuePair.PropertyChanged([weakThis = get_weak()](auto& sender, const PropertyChangedEventArgs& args) {
            if (auto appVM{ weakThis.get() })
            {
                appVM->_NotifyChanges(L"FeatureKeyValuePair");
                const auto settingName{ args.PropertyName() };
                if (settingName == L"FeatureKey")
                {
                    const auto senderPair = sender.as<FeatureKeyValuePair>();
                    const auto senderKey = senderPair->FeatureKey();
                    if (appVM->_IsDefaultFeature(senderKey))
                    {
                        senderPair->FeatureValue(1);
                    }
                    else
                    {
                        senderPair->FeatureValue(0);
                    }
                }
            }
        });
        return featureKeyValuePair;
    }

    bool AppearanceViewModel::_IsDefaultFeature(winrt::hstring featureKey)
    {
        for (const auto defaultFeature : DefaultFeatures)
        {
            if (defaultFeature == featureKey)
            {
                return true;
            }
        }
        return false;
    }

    DependencyProperty Appearances::_AppearanceProperty{ nullptr };

    Appearances::Appearances()
    {
        InitializeComponent();

        {
            using namespace winrt::Windows::Globalization::NumberFormatting;
            // > .NET rounds to 12 significant digits when displaying doubles, so we will [...]
            // ...obviously not do that, because this is an UI element for humans. This prevents
            // issues when displaying 32-bit floats, because WinUI is unaware about their existence.
            IncrementNumberRounder rounder;
            rounder.Increment(1e-6);

            for (const auto& box : { _fontSizeBox(), _lineHeightBox() })
            {
                // BODGY: Depends on WinUI internals.
                box.NumberFormatter().as<DecimalFormatter>().NumberRounder(rounder);
            }
        }

        INITIALIZE_BINDABLE_ENUM_SETTING(CursorShape, CursorStyle, winrt::Microsoft::Terminal::Core::CursorStyle, L"Profile_CursorShape", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(AdjustIndistinguishableColors, AdjustIndistinguishableColors, winrt::Microsoft::Terminal::Core::AdjustTextMode, L"Profile_AdjustIndistinguishableColors", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(BackgroundImageStretchMode, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, L"Profile_BackgroundImageStretchMode", L"Content");

        // manually add Custom FontWeight option. Don't add it to the Map
        INITIALIZE_BINDABLE_ENUM_SETTING(FontWeight, FontWeight, uint16_t, L"Profile_FontWeight", L"Content");
        _CustomFontWeight = winrt::make<EnumEntry>(RS_(L"Profile_FontWeightCustom/Content"), winrt::box_value<uint16_t>(0u));
        _FontWeightList.Append(_CustomFontWeight);

        if (!_AppearanceProperty)
        {
            _AppearanceProperty =
                DependencyProperty::Register(
                    L"Appearance",
                    xaml_typename<Editor::AppearanceViewModel>(),
                    xaml_typename<Editor::Appearances>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &Appearances::_ViewModelChanged } });
        }

        // manually keep track of all the Background Image Alignment buttons
        _BIAlignmentButtons.at(0) = BIAlign_TopLeft();
        _BIAlignmentButtons.at(1) = BIAlign_Top();
        _BIAlignmentButtons.at(2) = BIAlign_TopRight();
        _BIAlignmentButtons.at(3) = BIAlign_Left();
        _BIAlignmentButtons.at(4) = BIAlign_Center();
        _BIAlignmentButtons.at(5) = BIAlign_Right();
        _BIAlignmentButtons.at(6) = BIAlign_BottomLeft();
        _BIAlignmentButtons.at(7) = BIAlign_Bottom();
        _BIAlignmentButtons.at(8) = BIAlign_BottomRight();

        // apply automation properties to more complex setting controls
        for (const auto& biButton : _BIAlignmentButtons)
        {
            const auto tooltip{ ToolTipService::GetToolTip(biButton) };
            Automation::AutomationProperties::SetName(biButton, unbox_value<hstring>(tooltip));
        }

        const auto showAllFontsCheckboxTooltip{ ToolTipService::GetToolTip(ShowAllFontsCheckbox()) };
        Automation::AutomationProperties::SetFullDescription(ShowAllFontsCheckbox(), unbox_value<hstring>(showAllFontsCheckboxTooltip));

        const auto backgroundImgCheckboxTooltip{ ToolTipService::GetToolTip(UseDesktopImageCheckBox()) };
        Automation::AutomationProperties::SetFullDescription(UseDesktopImageCheckBox(), unbox_value<hstring>(backgroundImgCheckboxTooltip));

        _FontAxesNames = winrt::single_threaded_observable_vector<winrt::hstring>();
        FontAxesNamesCVS().Source(_FontAxesNames);

        _FontFeaturesNames = winrt::single_threaded_observable_vector<hstring>();
        FontFeaturesNamesCVS().Source(_FontFeaturesNames);

        INITIALIZE_BINDABLE_ENUM_SETTING(IntenseTextStyle, IntenseTextStyle, winrt::Microsoft::Terminal::Settings::Model::IntenseStyle, L"Appearance_IntenseTextStyle", L"Content");
    }

    IObservableVector<Editor::Font> Appearances::FilteredFontList()
    {
        if (!_filteredFonts)
        {
            _updateFilteredFontList();
        }
        return _filteredFonts;
    }

    // Method Description:
    // - Determines whether we should show the list of all the fonts, or we should just show monospace fonts
    bool Appearances::ShowAllFonts() const noexcept
    {
        return _ShowAllFonts;
    }

    void Appearances::ShowAllFonts(const bool value)
    {
        if (_ShowAllFonts != value)
        {
            _ShowAllFonts = value;
            _filteredFonts = nullptr;
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"FilteredFontList" });
        }
    }

    void Appearances::FontFaceBox_GotFocus(const Windows::Foundation::IInspectable& sender, const RoutedEventArgs&)
    {
        _updateFontNameFilter({});
        sender.as<AutoSuggestBox>().IsSuggestionListOpen(true);
    }

    void Appearances::FontFaceBox_LostFocus(const IInspectable& sender, const RoutedEventArgs&)
    {
        const auto appearance = Appearance();
        const auto fontSpec = sender.as<AutoSuggestBox>().Text();

        if (fontSpec.empty())
        {
            appearance.ClearFontFace();
        }
        else
        {
            appearance.FontFace(fontSpec);
        }

        // TODO: Any use of FindFontWithLocalizedName is broken and requires refactoring in time for version 1.21.
        const auto newFontFace = ProfileViewModel::FindFontWithLocalizedName(fontSpec);
        if (!newFontFace)
        {
            return;
        }

        _FontAxesNames.Clear();
        const auto axesTagsAndNames = newFontFace.FontAxesTagsAndNames();
        for (const auto tagAndName : axesTagsAndNames)
        {
            _FontAxesNames.Append(tagAndName.Value());
        }

        _FontFeaturesNames.Clear();
        const auto featuresTagsAndNames = newFontFace.FontFeaturesTagsAndNames();
        for (const auto tagAndName : featuresTagsAndNames)
        {
            _FontFeaturesNames.Append(tagAndName.Value());
        }

        // when the font face changes, we have to tell the view model to update the font axes/features vectors
        // since the new font may not have the same possible axes as the previous one
        Appearance().InitializeFontAxesVector();
        if (!Appearance().AreFontAxesAvailable())
        {
            // if the previous font had available font axes and the expander was expanded,
            // at this point the expander would be set to disabled so manually collapse it
            FontAxesContainer().SetExpanded(false);
            FontAxesContainer().HelpText(RS_(L"Profile_FontAxesUnavailable/Text"));
        }
        else
        {
            FontAxesContainer().HelpText(RS_(L"Profile_FontAxesAvailable/Text"));
        }

        Appearance().InitializeFontFeaturesVector();
        if (!Appearance().AreFontFeaturesAvailable())
        {
            // if the previous font had available font features and the expander was expanded,
            // at this point the expander would be set to disabled so manually collapse it
            FontFeaturesContainer().SetExpanded(false);
            FontFeaturesContainer().HelpText(RS_(L"Profile_FontFeaturesUnavailable/Text"));
        }
        else
        {
            FontFeaturesContainer().HelpText(RS_(L"Profile_FontFeaturesAvailable/Text"));
        }
    }

    void Appearances::FontFaceBox_SuggestionChosen(const AutoSuggestBox& sender, const AutoSuggestBoxSuggestionChosenEventArgs& args)
    {
        const auto font = unbox_value<Editor::Font>(args.SelectedItem());
        const auto fontName = font.Name();
        auto fontSpec = sender.Text();

        const std::wstring_view fontSpecView{ fontSpec };
        if (const auto idx = fontSpecView.rfind(L','); idx != std::wstring_view::npos)
        {
            const auto prefix = fontSpecView.substr(0, idx);
            const auto suffix = std::wstring_view{ fontName };
            fontSpec = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}, {}"), prefix, suffix) };
        }
        else
        {
            fontSpec = fontName;
        }

        sender.Text(fontSpec);
    }

    void Appearances::FontFaceBox_TextChanged(const AutoSuggestBox& sender, const AutoSuggestBoxTextChangedEventArgs& args)
    {
        if (args.Reason() != AutoSuggestionBoxTextChangeReason::UserInput)
        {
            return;
        }

        const auto fontSpec = sender.Text();
        std::wstring_view filter{ fontSpec };

        // Find the last font name in the font, spec, list.
        if (const auto idx = filter.rfind(L','); idx != std::wstring_view::npos)
        {
            filter = filter.substr(idx + 1);
        }

        filter = til::trim(filter, L' ');
        _updateFontNameFilter(filter);
    }

    void Appearances::_updateFontNameFilter(std::wstring_view filter)
    {
        if (_fontNameFilter != filter)
        {
            _filteredFonts = nullptr;
            _fontNameFilter = filter;
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"FilteredFontList" });
        }
    }

    void Appearances::_updateFilteredFontList()
    {
        _filteredFonts = _ShowAllFonts ? ProfileViewModel::CompleteFontList() : ProfileViewModel::MonospaceFontList();

        if (_fontNameFilter.empty())
        {
            return;
        }

        std::vector<Editor::Font> filtered;
        filtered.reserve(_filteredFonts.Size());

        for (const auto& font : _filteredFonts)
        {
            const auto name = font.Name();
            bool match = til::contains_linguistic_insensitive(name, _fontNameFilter);

            if (!match)
            {
                const auto localizedName = font.LocalizedName();
                match = localizedName != name && til::contains_linguistic_insensitive(localizedName, _fontNameFilter);
            }

            if (match)
            {
                filtered.emplace_back(font);
            }
        }

        _filteredFonts = winrt::single_threaded_observable_vector(std::move(filtered));
    }

    void Appearances::_ViewModelChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*args*/)
    {
        const auto& obj{ d.as<Editor::Appearances>() };
        get_self<Appearances>(obj)->_UpdateWithNewViewModel();
    }

    void Appearances::_UpdateWithNewViewModel()
    {
        if (const auto appearance = Appearance())
        {
            const auto& biAlignmentVal{ static_cast<int32_t>(appearance.BackgroundImageAlignment()) };
            for (const auto& biButton : _BIAlignmentButtons)
            {
                biButton.IsChecked(biButton.Tag().as<int32_t>() == biAlignmentVal);
            }

            FontAxesCVS().Source(Appearance().FontAxesVector());
            FontAxesContainer().HelpText(appearance.AreFontAxesAvailable() ? RS_(L"Profile_FontAxesAvailable/Text") : RS_(L"Profile_FontAxesUnavailable/Text"));

            FontFeaturesCVS().Source(Appearance().FontFeaturesVector());
            FontFeaturesContainer().HelpText(appearance.AreFontFeaturesAvailable() ? RS_(L"Profile_FontFeaturesAvailable/Text") : RS_(L"Profile_FontFeaturesUnavailable/Text"));

            _ViewModelChangedRevoker = appearance.PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
                const auto settingName{ args.PropertyName() };
                if (settingName == L"CursorShape")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
                }
                else if (settingName == L"DarkColorSchemeName" || settingName == L"LightColorSchemeName")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
                }
                else if (settingName == L"BackgroundImageStretchMode")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
                }
                else if (settingName == L"BackgroundImageAlignment")
                {
                    _UpdateBIAlignmentControl(static_cast<int32_t>(appearance.BackgroundImageAlignment()));
                }
                else if (settingName == L"FontWeight")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
                }
                else if (settingName == L"IntenseTextStyle")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentIntenseTextStyle" });
                }
                else if (settingName == L"AdjustIndistinguishableColors")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentAdjustIndistinguishableColors" });
                }
                else if (settingName == L"ShowProportionalFontWarning")
                {
                    PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ShowProportionalFontWarning" });
                }
                // YOU THERE ADDING A NEW APPEARANCE SETTING
                // Make sure you add a block like
                //
                //   else if (settingName == L"MyNewSetting")
                //   {
                //       PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentMyNewSetting" });
                //   }
                //
                // To make sure that changes to the AppearanceViewModel will
                // propagate back up to the actual UI (in Appearances). The
                // CurrentMyNewSetting properties are the ones that are bound in
                // XAML. If you don't do this right (or only raise a property
                // changed for "MyNewSetting"), then things like the reset
                // button won't work right.
            });

            // make sure to send all the property changed events once here
            // we do this in the case an old appearance was deleted and then a new one is created,
            // the old settings need to be updated in xaml
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentBackgroundImageStretchMode" });
            _UpdateBIAlignmentControl(static_cast<int32_t>(appearance.BackgroundImageAlignment()));
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentFontWeight" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ShowAllFonts" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentIntenseTextStyle" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"CurrentAdjustIndistinguishableColors" });
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ShowProportionalFontWarning" });
        }
    }

    fire_and_forget Appearances::BackgroundImage_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            Appearance().BackgroundImagePath(file);
        }
    }

    void Appearances::BIAlignment_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        if (const auto& button{ sender.try_as<Primitives::ToggleButton>() })
        {
            if (const auto& tag{ button.Tag().try_as<int32_t>() })
            {
                // Update the Appearance's value and the control
                Appearance().BackgroundImageAlignment(static_cast<ConvergedAlignment>(*tag));
                _UpdateBIAlignmentControl(*tag);
            }
        }
    }

    // Method Description:
    // - Resets all of the buttons to unchecked, and checks the one with the provided tag
    // Arguments:
    // - val - the background image alignment (ConvergedAlignment) that we want to represent in the control
    void Appearances::_UpdateBIAlignmentControl(const int32_t val)
    {
        for (const auto& biButton : _BIAlignmentButtons)
        {
            if (const auto& biButtonAlignment{ biButton.Tag().try_as<int32_t>() })
            {
                biButton.IsChecked(biButtonAlignment == val);
            }
        }
    }

    void Appearances::DeleteAxisKeyValuePair_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        if (const auto& button{ sender.try_as<Controls::Button>() })
        {
            if (const auto& tag{ button.Tag().try_as<winrt::hstring>() })
            {
                Appearance().DeleteAxisKeyValuePair(tag.value());
            }
        }
    }

    void Appearances::AddNewAxisKeyValuePair_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        Appearance().AddNewAxisKeyValuePair();
    }

    void Appearances::DeleteFeatureKeyValuePair_Click(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        if (const auto& button{ sender.try_as<Controls::Button>() })
        {
            if (const auto& tag{ button.Tag().try_as<hstring>() })
            {
                Appearance().DeleteFeatureKeyValuePair(tag.value());
            }
        }
    }

    void Appearances::AddNewFeatureKeyValuePair_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        Appearance().AddNewFeatureKeyValuePair();
    }

    bool Appearances::IsVintageCursor() const
    {
        return Appearance().CursorShape() == Core::CursorStyle::Vintage;
    }

    IInspectable Appearances::CurrentFontWeight() const
    {
        // if no value was found, we have a custom value
        const auto maybeEnumEntry{ _FontWeightMap.TryLookup(Appearance().FontWeight().Weight) };
        return maybeEnumEntry ? maybeEnumEntry : _CustomFontWeight;
    }

    void Appearances::CurrentFontWeight(const IInspectable& enumEntry)
    {
        if (auto ee = enumEntry.try_as<Editor::EnumEntry>())
        {
            if (ee != _CustomFontWeight)
            {
                const auto weight{ winrt::unbox_value<uint16_t>(ee.EnumValue()) };
                const Windows::UI::Text::FontWeight setting{ weight };
                Appearance().FontWeight(setting);

                // Appearance does not have observable properties
                // So the TwoWay binding doesn't update on the State --> Slider direction
                FontWeightSlider().Value(weight);
            }
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"IsCustomFontWeight" });
        }
    }

    bool Appearances::IsCustomFontWeight()
    {
        // Use SelectedItem instead of CurrentFontWeight.
        // CurrentFontWeight converts the Appearance's value to the appropriate enum entry,
        // whereas SelectedItem identifies which one was selected by the user.
        return FontWeightComboBox().SelectedItem() == _CustomFontWeight;
    }

}
