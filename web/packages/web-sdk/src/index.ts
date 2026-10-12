import { ModelsTmpl, type Features as RawFeatures } from './utils/common';
import { type GigaEdgeSpenchAudioNode as RawGigaEdgeSpenchAudioNode } from './audioNode';
import { DEFAULT_MODELS } from './utils/resources';

export { default as GigaEdgeSpenchSDK } from './sdk';
export { default as GigaEdgeSpenchError } from './utils/error';
export { LoadError } from './loader';
export { SetFeaturesFailedError } from './audioNode';

export type Features<M extends ModelsTmpl = typeof DEFAULT_MODELS> = RawFeatures<M>;
export type GigaEdgeSpenchAudioNode<M extends ModelsTmpl = typeof DEFAULT_MODELS> = RawGigaEdgeSpenchAudioNode<M>;
