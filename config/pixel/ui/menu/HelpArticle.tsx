import * as React from '@/libs/react';

import { Layout } from '../components/Layout';
import { Text } from '@/libs/nativeComponents/Text';
import { Rect } from '@/libs/nativeComponents/Rect';
import { rgb } from '@/libs/ui';
import { A2, A3, A4, B2, B3, B4, ScreenHeight, W1_4, W2_4, W3_4, menuTextColor } from '../constants';

export type Props = {
    name: string;
    title: string;
    body: string;
};

export function HelpArticle({ name, title, body }: Props) {
    return (
        <Layout
            viewName={name}
            color={rgb(20, 20, 20)}
            title={`Help · ${title}`}
            content={
                <>
                    <Rect bounds={[0, 20, 320, ScreenHeight - 40]} color="background" />
                    <Text
                        text={body}
                        bounds={[6, 24, 308, ScreenHeight - 56]}
                        color={menuTextColor}
                        keys={[
                            { key: A2, action: 'setView:&previous' },
                            { key: A3, action: '.scroll:-1' },
                            { key: B3, action: '.scroll' },
                            { key: B2, action: 'setView:HelpScreen' },
                            { key: B4, action: 'setView:HelpScreen' },
                        ]}
                    />

                    <Text text="Back (A2)" bounds={[0, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="Up (A3)" bounds={[W1_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="Down (B3)" bounds={[W2_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                    <Text text="Enter (A4)" bounds={[W3_4, ScreenHeight - 18, W1_4, 16]} centered color={menuTextColor} />
                </>
            }
        />
    );
}


