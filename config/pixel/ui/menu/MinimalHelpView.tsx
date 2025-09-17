import * as React from '@/libs/react';

import { Layout } from '../components/Layout';
import { Text } from '@/libs/nativeComponents/Text';
import { Rect } from '@/libs/nativeComponents/Rect';
import { rgb } from '@/libs/ui';
import { A2, A3, A4, B2, B3, B4, ScreenHeight, W1_4, W2_4, W3_4, helpContext, helpInstrContext, helpInstrDrumContext, menuTextColor } from '../constants';
import { HiddenValue } from '@/libs/nativeComponents/HiddenValue';

export type Props = { name: string };

export function MinimalHelpView({ name }: Props) {
    return (
        <Layout
            viewName={name}
            color={rgb(20, 20, 20)}
            title="Help"
            content={
                <>
                    <Rect bounds={[0, 20, 320, ScreenHeight - 40]} color="background" />
                    <Text text="Help" bounds={[8, 24, 304, 16]} color={menuTextColor} />
                    <Text
                        text="Main Screen"
                        bounds={[8, 48, 304, 16]}
                        color={menuTextColor}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpContext, value: 1 }]}
                    />
                    <Text
                        text="Main Screen"
                        bounds={[8, 48, 304, 16]}
                        color={rgb(255, 255, 255)}
                        visibilityContext={[{ condition: 'SHOW_WHEN', index: helpContext, value: 1 }]}
                    />
                    <Text
                        text="Sequencer View"
                        bounds={[8, 68, 304, 16]}
                        color={menuTextColor}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpContext, value: 2 }]}
                    />
                    <Text
                        text="Sequencer View"
                        bounds={[8, 68, 304, 16]}
                        color={rgb(255, 255, 255)}
                        visibilityContext={[{ condition: 'SHOW_WHEN', index: helpContext, value: 2 }]}
                    />

                    {/* Instruments row */}
                    <Text
                        text="Instruments >"
                        bounds={[8, 88, 304, 16]}
                        color={menuTextColor}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpContext, value: 3 }]}
                    />
                    <Text
                        text="Instruments >"
                        bounds={[8, 88, 304, 16]}
                        color={rgb(255, 255, 255)}
                        visibilityContext={[{ condition: 'SHOW_WHEN', index: helpContext, value: 3 }]}
                    />

                    <Text text="Back (A2)" bounds={[0, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="" bounds={[W1_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="" bounds={[W2_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="" bounds={[W3_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />

                    <HiddenValue
                        keys={[
                            // Global back only when not inside a submenu
                            { key: A2, action: 'setView:&previous' },
                        ]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }]}
                    />
                    <HiddenValue
                        keys={[
                            // Back one level when inside instruments submenu
                            { key: A2, action: `setContext:${helpInstrContext}:0` },
                            { key: B2, action: `setContext:${helpInstrContext}:0` },
                        ]}
                        visibilityContext={[{ condition: 'SHOW_WHEN', index: helpInstrContext, value: 1 }]}
                    />

                    {/* Bootstrap selection when nothing is selected (helpContext=0) */}
                    <HiddenValue
                        keys={[{ key: A3, action: `setContext:${helpContext}:1` }, { key: B2, action: `setContext:${helpContext}:1` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 0 }]}
                    />
                    <HiddenValue
                        keys={[{ key: B3, action: `setContext:${helpContext}:2` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 0 }]}
                    />
                    <HiddenValue
                        keys={[{ key: B4, action: `setContext:${helpContext}:3` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 0 }]}
                    />

                    {/* Navigation among top-level rows when not in submenu */}
                    {/* Down from Main -> Sequencer */}
                    <HiddenValue
                        keys={[{ key: B3, action: `setContext:${helpContext}:2` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 1 }]}
                    />
                    {/* Down from Sequencer -> Instruments */}
                    <HiddenValue
                        keys={[{ key: B3, action: `setContext:${helpContext}:3` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 2 }]}
                    />
                    {/* Up from Instruments -> Sequencer */}
                    <HiddenValue
                        keys={[{ key: A3, action: `setContext:${helpContext}:2` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 3 }]}
                    />
                    {/* Up from Sequencer -> Main */}
                    <HiddenValue
                        keys={[{ key: A3, action: `setContext:${helpContext}:1` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 2 }]}
                    />
                    {/* Enter on selected top-level item when not in submenu */}
                    <HiddenValue
                        keys={[{ key: A4, action: 'setView:HelpMain' }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 1 }]}
                    />
                    <HiddenValue
                        keys={[{ key: A4, action: 'setView:HelpSequencer' }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 2 }]}
                    />
                    <HiddenValue
                        keys={[{ key: A4, action: `setContext:${helpInstrContext}:1` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 3 }]}
                    />

                    {/* Also allow Right (B4) to open Instruments when selected */}
                    <HiddenValue
                        keys={[{ key: B4, action: `setContext:${helpInstrContext}:1` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN_NOT', index: helpInstrContext, value: 1 }, { condition: 'SHOW_WHEN', index: helpContext, value: 3 }]}
                    />

                    {/* Instruments submenu (Drum) */}
                    <Text
                        text="Drum >"
                        bounds={[24, 108, 288, 16]}
                        color={menuTextColor}
                        visibilityContext={[{ condition: 'SHOW_WHEN', index: helpInstrContext, value: 1 }]}
                    />
                    <HiddenValue
                        keys={[{ key: B4, action: `setContext:${helpInstrDrumContext}:1` }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN', index: helpInstrContext, value: 1 }]}
                    />
                    <Text
                        text="Metalic"
                        bounds={[40, 128, 272, 16]}
                        color={menuTextColor}
                        visibilityContext={[{ condition: 'SHOW_WHEN', index: helpInstrDrumContext, value: 1 }]}
                    />
                    <HiddenValue
                        keys={[{ key: A4, action: 'setView:HelpInstruments' }]}
                        visibilityContext={[{ condition: 'SHOW_WHEN', index: helpInstrDrumContext, value: 1 }]}
                    />
                </>
            }
        />
    );
}


