import * as React from '@/libs/react';

import { Layout } from '../components/Layout';
import { Text } from '@/libs/nativeComponents/Text';
import { Rect } from '@/libs/nativeComponents/Rect';
import { HiddenValue } from '@/libs/nativeComponents/HiddenValue';
import { rgb } from '@/libs/ui';
import { A2, A3, A4, B2, B3, B4, ScreenHeight, W1_4, W2_4, W3_4, menuTextColor } from '../constants';

type Item = { id: string; title: string; articleView: string };

const items: Item[] = [
    { id: 'main', title: 'Main Screen', articleView: 'HelpMain' },
    { id: 'sequencer', title: 'Sequencer View', articleView: 'HelpSequencer' },
    { id: 'banks', title: 'Bank View', articleView: 'HelpBanks' },
    { id: 'instruments', title: 'Instruments & Engines', articleView: 'HelpInstruments' },
];

export type Props = { name: string };

export function HelpView({ name }: Props) {
    return (
        <Layout
            viewName={name}
            color={rgb(20, 20, 20)}
            title="Help"
            content={
                <>
                    <Rect bounds={[0, 20, 320, ScreenHeight - 40]} color="background" />

                    {items.map((it, idx) => (
                        <Text
                            text={`• ${it.title}`}
                            bounds={[8, 24 + idx * 20, 304, 16]}
                            color={menuTextColor}
                        />
                    ))}

                    <Text text="Back (A2)" bounds={[0, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="Left (B2)" bounds={[W1_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="Right (B4)" bounds={[W2_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="Enter (A4)" bounds={[W3_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />

                    <HiddenValue keys={[
                        { key: A2, action: 'setView:&previous' },
                        { key: A4, action: 'setView:HelpMain' },
                    ]} />
                </>
            }
        />
    );
}


