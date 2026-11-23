#pragma once
// Supported FH3 1.0.125.0 native layout, retained so the tile can be
// published during metadata initialization before the game caches its pages.
namespace garage {inline constexpr std::string_view nativeDesignsMarkup=R"UGLIVERY(<Page
    xmlns:local="clr-namespace:ForzaUI"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" 
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008" 
    xmlns:System="clr-namespace:System;assembly=mscorlib"
    x:Class="ForzaUI.A_LiveryModeSelect"
    mc:Ignorable="d"
    Style="{DynamicResource A_Style_Page_Generic}"
    DataContext="{Binding RelativeSource={RelativeSource Self}}">
        
    <Page.Resources>
        <ResourceDictionary>
            <!--
            <ResourceDictionary.MergedDictionaries>
                <ResourceDictionary Source="/Resources/Anthem/Shared_TiledMenus.xaml"/>
                <ResourceDictionary Source="/Resources/Anthem/Shared_Bars.xaml"/>
            </ResourceDictionary.MergedDictionaries>
            -->
            <!-- Tile Coordinates -->
            <System:Double x:Key="A_Double_TileAreaWidth">1692</System:Double>
            <System:Double x:Key="A_Double_TileAreaHeight">564</System:Double>
                        
            <Style x:Key="A_Style_Canvas_BaseMenu" TargetType="{x:Type Canvas}">
                <Setter Property="HorizontalAlignment" Value="Center"/>
                <Setter Property="VerticalAlignment" Value="Center"/>
                <Setter Property="Width" Value="{StaticResource A_Double_TileAreaWidth}"/>
                <Setter Property="Height" Value="{StaticResource A_Double_TileAreaHeight}"/>
                <Setter Property="RenderTransform">
                    <Setter.Value>
                        <TransformGroup>
                            <ScaleTransform/>
                            <SkewTransform/>
                            <RotateTransform/>
                            <TranslateTransform/>
                        </TransformGroup>
                    </Setter.Value>
                </Setter>
            </Style>

            <!-- Non-Live TileData -->
            <local:TileMenuDataItem x:Key="A_MenuDataItem_FindDesigns" Heading="{local:Loc Livery.IDS_TileTitle_FindDesigns}" Description="{local:Loc Livery.IDS_TileDesc_FindDesigns}" UseAlternateColors="True" Icon="/ForzaUI;component/Textures/Anthem/PauseMenuTileBackgrounds/FindNewDesign.png" LockedIcon="/ForzaUI;component/Textures/Anthem/PauseMenuTileBackgrounds/FindNewDesign_Locked.png" />
            <local:TileMenuDataItem x:Key="A_MenuDataItem_LoadDesign" Heading="{local:Loc Livery.IDS_TileTitle_LoadDesign}" Description="{local:Loc Livery.IDS_TileDesc_LoadDesign}" Icon="/ForzaUI;component/Textures/Anthem/PauseMenuTileBackgrounds/MyDesigns.png" LockedIcon="/ForzaUI;component/Textures/Anthem/PauseMenuTileBackgrounds/MyDesigns_Locked.png" />

            <local:TileMenuDataItem x:Key="A_MenuDataItem_CarSelect"          Heading="{local:Loc Livery.IDS_TileTitle_CarSelect}"    Icon="/ForzaUI;component/Textures/Anthem/Icons/Livery/Car_Select.png" />
            <local:TileMenuDataItem x:Key="A_MenuDataItem_PaintCar"           Heading="{local:Loc Livery.IDS_TileTitle_PaintCar}"     Icon="/ForzaUI;component/Textures/Anthem/Icons/Livery/Paint_Car_Icon.png" />
            <local:TileMenuDataItem x:Key="A_MenuDataItem_LoadVinylGroup"     Heading="{local:Loc Livery.IDS_TileTitle_LoadGroup}"    />
            <local:TileMenuDataItem x:Key="A_MenuDataItem_CreateVinylGroup"   Heading="{local:Loc Livery.IDS_TileTitle_CreateGroup}"  />
            <local:TileMenuDataItem x:Key="A_MenuDataItem_ApplyDecals"        Heading="{local:Loc Livery.IDS_TileTitle_CreateDesign}" Icon="/ForzaUI;component/Textures/Anthem/Icons/Livery/Apply_Decal_Icon.png" />
            <local:TileMenuDataItem x:Key="A_MenuDataItem_ErasePaintDecals"   Heading="{local:Loc Livery.IDS_TileTitle_EraseDesign}"  Icon="/ForzaUI;component/Textures/Anthem/Icons/Livery/Erase_Paint_Icon.png" />
        </ResourceDictionary>
    </Page.Resources>
    
    <local:UIPageTransitionManager.TransitionManager>
        <local:UIPageTransitionManager Preset="{StaticResource A_UIPageTransitionManagerPreset_Menu}"/>
    </local:UIPageTransitionManager.TransitionManager>
            
    <Grid>
        <!-- Background Blur -->
        <local:FZMaterial Style="{StaticResource A_Style_FZMaterial_Glass_ScreenBackground}" />
        
        <local:PlayerBarControl Style="{StaticResource A_Style_PlayerBarControl}"
                        x:Name="TopBar" />
        <ContentControl x:Name="BottomBar" Style="{StaticResource A_Style_ButtonBar}">
            <ContentControl x:Name="DefaultHelpPanel"
                    local:ControllerButtonPanel.ADescription="{local:Loc HelpButtons.IDS_Select}"
                    local:ControllerButtonPanel.BDescription="{local:Loc HelpButtons.IDS_Back}"
                    Style="{StaticResource A_Style_ContentControl_DefaultButtonBar}" />
        </ContentControl>
    
        <Border Style="{StaticResource A_Style_Border_ScenePadding}">
            <!-- States to enable easy editing in blend -->
            <VisualStateManager.VisualStateGroups>
                <VisualStateGroup x:Name="Screens">
                    <VisualState x:Name="Home">
                        <Storyboard>
                            <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.RenderTransform).(TransformGroup.Children)[3].(TranslateTransform.X)" Storyboard.TargetName="PART_SCOPE_Home">
                                <EasingDoubleKeyFrame KeyTime="0" Value="0"/>
                            </DoubleAnimationUsingKeyFrames>
                        </Storyboard>
                    </VisualState>
                </VisualStateGroup>
            </VisualStateManager.VisualStateGroups>	

            <Grid x:Name="LayoutRoot" KeyboardNavigation.DirectionalNavigation="Contained">
                <Grid Style="{StaticResource A_Style_FrameworkElement_ScreenTilt}">
                    <StackPanel Orientation="Vertical" VerticalAlignment="Center" HorizontalAlignment="Center">
                        <StackPanel Orientation="Horizontal" Margin="0,0,3,0" Style="{StaticResource A_Style_FrameworkElement_DefaultTransition_0}">
                            <ContentControl Content="{local:Loc Livery.IDS_MenuTitle_LiveryMode}"
                                            Style="{StaticResource A_Style_ContentControl_ScreenTitleBlock}"
                                            local:BindingProxy.A="/ForzaUI;component/Textures/Anthem/Icons/Titles/Painting.png"/>
                        </StackPanel>

                        <StackPanel x:Name="PART_MenuContent">
                            <Canvas x:Name="PART_SCOPE_Home"
                                    Style="{StaticResource A_Style_Canvas_BaseMenu}">
                                <local:FZButton x:Name="PART_Button_PaintCar"
                                        DataContext="{StaticResource A_MenuDataItem_PaintCar}"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="0"
                                        local:TileMenuNavigationManager.GridY="0"
                                        Style="{StaticResource A_Style_Button_Tile_2x2}"
                                        Template="{StaticResource A_ControlTemplate_IconTile}"
                                        local:UIPageTransitionManager.TransitionIndex="1"
                                        local:BindingProxy.C="DisableMotionBlur" />
                                <local:FZButton x:Name="PART_Button_ApplyDecals"
                                        DataContext="{StaticResource A_MenuDataItem_ApplyDecals}"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="0"
                                        local:TileMenuNavigationManager.GridY="2"
                                        Style="{StaticResource A_Style_Button_Tile_2x2}"
                                        Template="{StaticResource A_ControlTemplate_IconTile}"
                                        local:UIPageTransitionManager.TransitionIndex="2"
                                        local:BindingProxy.C="DisableMotionBlur" />
                                <local:FZButton x:Name="PART_Button_FindDesign"
                                        DataContext="{StaticResource A_MenuDataItem_FindDesigns}"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="2"
                                        local:TileMenuNavigationManager.GridY="0"
                                        Style="{StaticResource A_Style_Button_Tile_6x2}"
                                        Template="{StaticResource A_ControlTemplate_LargeHorizontalImageTile}"
                                        local:UIPageTransitionManager.TransitionIndex="3"
                                        local:BindingProxy.C="DisableMotionBlur" />
                                <local:FZButton x:Name="PART_Button_CarSelect"
                                        DataContext="{StaticResource A_MenuDataItem_CarSelect}"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="2"
                                        local:TileMenuNavigationManager.GridY="2"
                                        Style="{StaticResource A_Style_Button_Tile_2x2}"
                                        Template="{StaticResource A_ControlTemplate_IconTile}"
                                        local:UIPageTransitionManager.TransitionIndex="4"
                                        local:BindingProxy.C="DisableMotionBlur" />

                                <local:InfographicTile Template="{StaticResource A_Template_Infographic}"
                                                       Style="{StaticResource A_Style_Infographics_Button}"
                                                       local:TileMenuNavigationManager.GridX="4"
                                                       local:TileMenuNavigationManager.GridY="2"
                                                       local:UIPageTransitionManager.TransitionIndex="5"
                                                       x:Name="PART_Button_Infographic"
                                                       local:BindingProxy.C="DisableMotionBlur"/>
                                <local:FZButton x:Name="PART_Button_LoadDesign"
                                        DataContext="{StaticResource A_MenuDataItem_LoadDesign}"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="6"
                                        local:TileMenuNavigationManager.GridY="2"
                                        Style="{StaticResource A_Style_Button_Tile_6x2}"
                                        Template="{StaticResource A_ControlTemplate_LargeHorizontalImageTile}"
                                        local:UIPageTransitionManager.TransitionIndex="6"
                                        local:BindingProxy.C="DisableMotionBlur" />
                                <local:FZButton x:Name="PART_Button_CreateVinylGroup"
                                        DataContext="{StaticResource A_MenuDataItem_CreateVinylGroup}"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="8"
                                        local:TileMenuNavigationManager.GridY="0"
                                        Style="{StaticResource A_Style_Button_Tile_2x1}"
                                        Template="{StaticResource A_ControlTemplate_SmallHorizontalIconTile}"
                                        local:UIPageTransitionManager.TransitionIndex="7"
                                        local:BindingProxy.C="DisableMotionBlur" />
                                <local:FZButton x:Name="PART_Button_LoadVinylGroup"
                                        DataContext="{StaticResource A_MenuDataItem_LoadVinylGroup}"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="8"
                                        local:TileMenuNavigationManager.GridY="1"
                                        Style="{StaticResource A_Style_Button_Tile_2x1}"
                                        Template="{StaticResource A_ControlTemplate_SmallHorizontalIconTile}"
                                        local:UIPageTransitionManager.TransitionIndex="8"
                                        local:BindingProxy.C="DisableMotionBlur" />

                                <local:FZButton x:Name="PART_Button_ErasePaintDecals"
                                        DataContext="{StaticResource A_MenuDataItem_ErasePaintDecals}"
                                        local:TileMenuNavigationManager.TileState="Enabled"
                                        local:TileMenuNavigationManager.GridX="10"
                                        local:TileMenuNavigationManager.GridY="0"
                                        Style="{StaticResource A_Style_Button_Tile_2x2}"
                                        Template="{StaticResource A_ControlTemplate_IconTile}"
                                        local:UIPageTransitionManager.TransitionIndex="9"
                                        local:BindingProxy.C="DisableMotionBlur" />
                            </Canvas>
                        </StackPanel>
                    </StackPanel>
                </Grid>
            </Grid>
        </Border>
    </Grid>
</Page>
)UGLIVERY";}
