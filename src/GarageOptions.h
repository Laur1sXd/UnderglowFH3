// Included in namespace garage after the native Color Select helpers.
constexpr size_t optionsVtable = 0x45a1ec0;
constexpr wchar_t brightnessOptionsTag[] = L"UG Brightness Options";
constexpr wchar_t effectsOptionsTag[] = L"UG Effects Options";
void* optionsOverlay;
void* optionsParent;
bool optionsShowSpeed;
PageFn originalOptionsUpdate;
void (*originalOptionsPrompt)(void*, int, Byte**, void*, int, Byte);
std::array<float, std::size(sliderRows)> optionsPreviewSliders{};
bool optionsPreviewActive;
struct OptionsEffectTrack {
    int effect{};
    Color first{}, second{};
    std::array<Byte*, 2> swatches{};
    std::array<unsigned, 6> colors{};
};
OptionsEffectTrack optionsEffectTrack;

bool hasSpeedOptions(Tab tab, int effect) {
    return tab >= oneColorEffects && tab <= multicolorEffects && effect != fx::kEffectStatic;
}

bool hasGarageOptions(int tab, int choice) {
    return tab >= primaryColors && tab <= multicolorEffects && choice != noUnderglowChoice;
}

void optionsPrompt(void* page, int button, Byte** label, void* action, int command, Byte close) {
    if (optionsParent && page == optionsOverlay && label && *label && (button == 0 || button == 1)) {
        if (auto* replacement = uiText(button == 0 ? L"Apply" : L"Back")) {
            release(*label); // The native registration consumes the caller's reference.
            originalOptionsPrompt(page, button, &replacement, action, command, close);
            return;
        }
    }
    originalOptionsPrompt(page, button, label, action, command, close);
}

std::string optionsTrigger(const char* tag, std::string_view setters) {
    return std::string(R"XAML(<DataTrigger Binding="{Binding Tag, RelativeSource={RelativeSource AncestorType=Page}}" Value=")XAML") +
        tag + "\">" + std::string(setters) + "</DataTrigger>";
}

bool patchOptionsMarkup(std::string_view source, std::string& result) {
    if (!installed.load() || source.find("x:Class=\"ForzaUI.A_ColorSelectSliders\"") == source.npos ||
        source.find("UG Brightness Options") != source.npos) return false;
    constexpr char brightnessTag[] = "UG Brightness Options", effectsTag[] = "UG Effects Options";
    constexpr auto collapse = R"XAML(<Setter Property="Visibility" Value="Collapsed" />)XAML";
    const auto hideForOptions = optionsTrigger(brightnessTag, collapse) + optionsTrigger(effectsTag, collapse);
    std::string markup(source);
    const auto track = markup.find("x:Name=\"TrackBackground\"");
    const auto trackStart = track == markup.npos ? markup.npos : markup.find("<Track x:Name=\"PART_Track\"", track);
    const auto trackEnd = trackStart == markup.npos ? markup.npos : markup.find("</Track>", trackStart);
    const auto trackTriggers = track == markup.npos ? markup.npos : markup.find("</ControlTemplate.Triggers>", track);
    if (trackEnd == markup.npos || trackTriggers == markup.npos || trackEnd >= trackTriggers) return false;
    const auto trackTrigger = [](const char* pageTag, const char* sliderName, std::string_view setters) {
        return std::string(R"XAML(<MultiDataTrigger><MultiDataTrigger.Conditions>
            <Condition Binding="{Binding Tag, RelativeSource={RelativeSource AncestorType=Page}}" Value=")XAML") + pageTag +
            R"XAML(" /><Condition Binding="{Binding Name, RelativeSource={RelativeSource Self}}" Value=")XAML" + sliderName +
            "\" /></MultiDataTrigger.Conditions>" + std::string(setters) + "</MultiDataTrigger>";
    };
    constexpr auto brightnessTrack = R"XAML(
            <Setter TargetName="TrackBackground" Property="Background"><Setter.Value>
                <LinearGradientBrush StartPoint="0,0.5" EndPoint="1,0.5">
                    <GradientStop Color="#FF111111" Offset="0" /><GradientStop Color="#FFFFFFFF" Offset="1" />
                </LinearGradientBrush>
            </Setter.Value></Setter>)XAML";
    constexpr auto showAnimation = R"XAML(<Setter TargetName="UnderglowEffectAnimation" Property="Visibility" Value="Visible" />)XAML";
    markup.insert(trackTriggers,
        trackTrigger(brightnessTag, "PART_HueSlider", brightnessTrack) +
        trackTrigger(effectsTag, "PART_HueSlider", brightnessTrack) +
        trackTrigger(effectsTag, "SaturationSlider", showAnimation));
    markup.insert(trackEnd + sizeof("</Track>") - 1, "</Grid>");
    markup.insert(trackStart, R"XAML(<Grid>
        <Border x:Name="UnderglowEffectAnimation" VerticalAlignment="Stretch"
                Visibility="Collapsed" IsHitTestVisible="False">
            <Border.Background><LinearGradientBrush StartPoint="0,0.5" EndPoint="1,0.5">
                <GradientStop Color="{Binding PreviousColor.PrimaryColor}" Offset="0" />
                <GradientStop Color="{Binding PreviousColor.SecondaryColor}" Offset="0.2" />
                <GradientStop Color="{Binding PreviousColor.StripeColor}" Offset="0.4" />
                <GradientStop Color="{Binding CurrentColor.PrimaryColor}" Offset="0.6" />
                <GradientStop Color="{Binding CurrentColor.SecondaryColor}" Offset="0.8" />
                <GradientStop Color="{Binding CurrentColor.StripeColor}" Offset="1" />
            </LinearGradientBrush></Border.Background>
        </Border>)XAML");

    const auto styleSlider = [&](const char* name, const char* label, const std::string& triggers) {
        const auto start = markup.find(std::string("<Slider x:Name=\"") + name + "\"");
        const auto end = start == markup.npos ? markup.npos : markup.find('>', start);
        if (end == markup.npos) return false;
        auto opening = markup.substr(start, end - start + 1);
        for (const auto& attribute : std::array<std::string, 3>{" Maximum=\"1\"",
                " Style=\"{StaticResource A_Style_ColorFineTuneSlider}\"",
                std::string(" Tag=\"{local:Loc Livery.IDS_") + label + "}\""}) {
            const auto at = opening.find(attribute);
            if (at == opening.npos) return false;
            opening.erase(at, attribute.size());
        }
        opening += R"XAML(<Slider.Style><Style TargetType="Slider" BasedOn="{StaticResource A_Style_ColorFineTuneSlider}">
            <Setter Property="Maximum" Value="1" /><Setter Property="Tag" Value="{local:Loc Livery.IDS_)XAML";
        opening += label;
        opening += "}\" /><Style.Triggers>" + triggers + "</Style.Triggers></Style></Slider.Style>";
        markup.replace(start, end - start + 1, opening);
        return true;
    };
    constexpr auto brightnessSetters = R"XAML(<Setter Property="Tag" Value="Brightness" />
        <Setter Property="Maximum" Value="300" /><Setter Property="SmallChange" Value="5" />)XAML";
    constexpr auto speedSetters = R"XAML(<Setter Property="Tag" Value="Effect Speed" />
        <Setter Property="Minimum" Value="0.1" /><Setter Property="Maximum" Value="3" />
        <Setter Property="SmallChange" Value="0.05" />)XAML";
    if (!styleSlider("PART_HueSlider", "Hue", optionsTrigger(brightnessTag, brightnessSetters) +
            optionsTrigger(effectsTag, brightnessSetters)) ||
        !styleSlider("SaturationSlider", "Saturation", optionsTrigger(brightnessTag, collapse) +
            optionsTrigger(effectsTag, speedSetters)) ||
        !styleSlider("BrightnessSlider", "Brightness", hideForOptions)) return false;

    const auto swatches = markup.find("<Control x:Name=\"PART_Control_CustomColorItem\"");
    const auto second = markup.find("<Control x:Name=\"PART_Control_CustomColorItem2\"", swatches);
    const auto swatchesEnd = second == markup.npos ? markup.npos : markup.find("/>", second);
    if (swatchesEnd == markup.npos) return false;
    markup.insert(swatchesEnd + 2, "</StackPanel>");
    markup.insert(swatches, R"XAML(<StackPanel Orientation="Horizontal"><StackPanel.Style>
        <Style TargetType="StackPanel"><Style.Triggers>)XAML" + hideForOptions +
        "</Style.Triggers></Style></StackPanel.Style>");

    constexpr std::string_view stockTitle = R"XAML(Content="{local:Loc ScreenTitles.IDS_Color_FineTune}")XAML";
    const auto title = markup.find(stockTitle);
    if (title == markup.npos) return false;
    markup.erase(title, stockTitle.size());
    const auto style = markup.find(R"XAML(Style="{StaticResource A_Style_ContentControl_ScreenTitleBlock}")XAML", title);
    if (style == markup.npos) return false;
    markup.erase(style, sizeof(R"XAML(Style="{StaticResource A_Style_ContentControl_ScreenTitleBlock}")XAML") - 1);
    const auto titleEnd = markup.find("/>", title);
    if (titleEnd == markup.npos) return false;
    constexpr auto optionsTitle = R"XAML(<Setter Property="Content" Value="UNDERGLOW OPTIONS" />)XAML";
    markup.replace(titleEnd, 2, R"XAML(><ContentControl.Style>
        <Style TargetType="ContentControl" BasedOn="{StaticResource A_Style_ContentControl_ScreenTitleBlock}">
            <Setter Property="Content" Value="{local:Loc ScreenTitles.IDS_Color_FineTune}" /><Style.Triggers>)XAML" +
        optionsTrigger(brightnessTag, optionsTitle) + optionsTrigger(effectsTag, optionsTitle) +
        "</Style.Triggers></Style></ContentControl.Style></ContentControl>");
    result = std::move(markup);
    return true;
}

// The native Fine Tune scene binds these float properties to its first two sliders.
void* optionsProperty(size_t index) {
    const auto* holder = native<const Byte* (*)()>(index == Brightness ? 0x15cfaa0 : 0x15cfe10)();
    return read<void*>(holder + 0x20);
}

bool setOptionsValue(void* page, size_t index, float display) {
    auto* property = optionsProperty(index);
    if (!property) return false;
    Byte* boxed = nullptr;
    native<Byte** (*)(Byte**, const float*)>(0x134abf0)(&boxed, &display);
    if (!boxed) return false;
    setValue(page, property, boxed);
    release(boxed);
    return true;
}

void readOptionsSliders(void* page) {
    auto values = optionsPreviewSliders;
    const size_t count = optionsShowSpeed ? std::size(sliderRows) : 1;
    for (size_t index = 0; index < count; ++index) {
        auto* property = optionsProperty(index);
        if (!property) continue;
        Byte* boxed = nullptr;
        native<Byte** (*)(void*, Byte**, void*)>(0x212f910)(page, &boxed, property);
        if (boxed) {
            const auto& row = sliderRows[index];
            const float value = read<float>(boxed + 0x28) / row.scale;
            if (std::isfinite(value)) values[index] = std::clamp(value, row.min, row.max);
            release(boxed);
        }
    }
    AcquireSRWLockExclusive(&settingsLock);
    optionsPreviewSliders = values;
    ReleaseSRWLockExclusive(&settingsLock);
}

void closeOptions(void* parent) {
    if (optionsParent != parent) return;
    optionsParent = nullptr;
    for (auto* swatch : optionsEffectTrack.swatches) release(swatch);
    optionsEffectTrack = {};
    AcquireSRWLockExclusive(&settingsLock);
    optionsPreviewActive = false;
    ReleaseSRWLockExclusive(&settingsLock);
}

bool saveOptionsSliders() {
    AcquireSRWLockExclusive(&settingsLock);
    const bool saved = updateIni([](const wchar_t* temp) {
        const size_t count = optionsShowSpeed ? std::size(sliderRows) : 1;
        for (size_t index = 0; index < count; ++index) {
            if (optionsPreviewSliders[index] == savedSettings.sliders[index]) continue;
            wchar_t value[32];
            swprintf_s(value, L"%.3f", static_cast<double>(optionsPreviewSliders[index]));
            if (!WritePrivateProfileStringW(section, sliderRows[index].iniKey, value, temp)) return false;
        }
        return true;
    });
    if (saved) {
        garageTuningChanged |= savedSettings.sliders[Brightness] != optionsPreviewSliders[Brightness] ||
            (optionsShowSpeed && savedSettings.sliders[Speed] != optionsPreviewSliders[Speed]);
        savedSettings.sliders[Brightness] = optionsPreviewSliders[Brightness];
        if (optionsShowSpeed) savedSettings.sliders[Speed] = optionsPreviewSliders[Speed];
        draftSettings.sliders = savedSettings.sliders;
    }
    ReleaseSRWLockExclusive(&settingsLock);
    return saved;
}

void acceptOptions(PopupAction* action) {
    if (action->popup != optionsOverlay || action->request.page != optionsParent ||
        designCar(action->request.page) != action->request.id) return;
    readOptionsSliders(action->popup);
    const bool saved = saveOptionsSliders();
    log(saved ? "Garage Underglow Options accepted." : "Could not save Garage Underglow Options.");
}

void* optionsActionTable[]{reinterpret_cast<void*>(copyPopup), reinterpret_cast<void*>(copyPopup),
    reinterpret_cast<void*>(acceptOptions), reinterpret_cast<void*>(popupType),
    reinterpret_cast<void*>(deletePopup), reinterpret_cast<void*>(popupData)};

void updateOptionsEffectTrack(double seconds) {
    const auto& first = optionsEffectTrack.first;
    const auto& second = optionsEffectTrack.second;
    const fx::AudioFrame silentRadio;
    const fx::EffectFrame frame{optionsPreviewSliders[Speed], {first.r, first.g, first.b},
        {second.r, second.g, second.b}, silentRadio};
    // Sample the car's actual ring so each effect retains its own timing and shape.
    constexpr std::array<size_t, 6> sampleIndices{0, 6, 11, 16, 22, 31};
    constexpr size_t channels[]{0x1249f20, 0x1249fd0, 0x124a080}; // Primary, Secondary, Stripe
    for (size_t sample = 0; sample < sampleIndices.size(); ++sample) {
        const size_t index = sampleIndices[sample];
        const auto zone = static_cast<size_t>(std::upper_bound(zoneStarts.begin(), zoneStarts.end(), index) - zoneStarts.begin() - 1);
        auto pixel = fx::pixel(static_cast<int>(zone), static_cast<int>(index - zoneStarts[zone]), static_cast<int>(zoneSizes[zone]));
        pixel.id = pixel.gi = static_cast<int>(index);
        pixel.gn = static_cast<int>(lightCount);
        pixel.ring = (static_cast<float>(index) + 0.5f) / static_cast<float>(lightCount);
        float rgb[3], level;
        fx::effect_now(frame, optionsEffectTrack.effect, pixel, seconds, rgb, &level);
        const unsigned color = encodeColor({rgb[0] * level, rgb[1] * level, rgb[2] * level});
        if (color == optionsEffectTrack.colors[sample]) continue;
        auto* swatch = optionsEffectTrack.swatches[sample / std::size(channels)];
        if (swatch && setSwatchColor(swatch, channels[sample % std::size(channels)], color))
            optionsEffectTrack.colors[sample] = color;
    }
}

void optionsUpdate(void* page) {
    if (page != optionsOverlay) {
        originalOptionsUpdate(page);
        return;
    }
    // Run the base scene update without the Fine Tune paint-preview writer.
    native<PageFn>(0x14a43d0)(page);
    if (!optionsParent) return;
    readOptionsSliders(page);
    if (optionsShowSpeed) updateOptionsEffectTrack(effectSeconds(GetTickCount64()));
}

void openOptions(void* parent, Id id, Tab tab, int effect, const Look& appearance) {
    if (optionsParent) return;
    auto* popup = native<Byte* (*)(void*)>(0x14b8910)(parent);
    if (!popup || read<const Byte*>(popup) != game + optionsVtable) return;
    ++*reinterpret_cast<int*>(popup + 0x10);
    optionsOverlay = popup;
    optionsParent = parent;
    optionsShowSpeed = hasSpeedOptions(tab, effect);
    AcquireSRWLockExclusive(&settingsLock);
    optionsPreviewSliders = savedSettings.sliders;
    ReleaseSRWLockExclusive(&settingsLock);
    if (optionsShowSpeed) {
        optionsEffectTrack.effect = effect;
        optionsEffectTrack.first = resolvedColor(appearance.primary, appearance.rgb);
        optionsEffectTrack.second = effectSecondary(optionsEffectTrack.first, resolvedColor(appearance.secondary, appearance.rgb));
        optionsEffectTrack.colors.fill(UINT32_MAX);
        constexpr size_t properties[]{0x15cfcb0, 0x15cf9f0}; // PreviousColor, CurrentColor
        for (size_t i = 0; i < optionsEffectTrack.swatches.size(); ++i) {
            auto& swatch = optionsEffectTrack.swatches[i];
            native<Byte** (*)(Byte**)>(0x123ecb0)(&swatch);
            const auto* holder = native<const Byte* (*)()>(properties[i])();
            if (swatch) setValue(popup, read<void*>(holder + 0x20), swatch);
        }
        updateOptionsEffectTrack(effectSeconds(GetTickCount64()));
    }
    tagPage(popup, optionsShowSpeed ? effectsOptionsTag : brightnessOptionsTag);
    const bool ready = setOptionsValue(popup, Brightness, optionsPreviewSliders[Brightness] * sliderRows[Brightness].scale) &&
        (!optionsShowSpeed || setOptionsValue(popup, Speed, optionsPreviewSliders[Speed] * sliderRows[Speed].scale));
    if (ready) {
        AcquireSRWLockExclusive(&settingsLock);
        optionsPreviewActive = true;
        ReleaseSRWLockExclusive(&settingsLock);
    }
    native<void (*)(void*, int)>(0x1598950)(popup, 1);
    if (ready) {
        PopupAction action{optionsActionTable, {parent, id}, popup};
        action.implementation = &action;
        native<void (*)(void*, int, PopupAction*, unsigned char)>(0x1598860)(popup, 2, &action, 1);
        log(optionsShowSpeed ? "Opened Garage Underglow Options with Brightness and Effect Speed." :
            "Opened Garage Underglow Options with Brightness.");
    }
    release(popup);
}
