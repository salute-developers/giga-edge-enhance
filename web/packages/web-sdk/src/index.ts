import { ModelsTmpl, type Features as RawFeatures } from './utils/common';
import { type GigaEdgeEnhanceAudioNode as RawGigaEdgeEnhanceAudioNode } from './audioNode';
import { DEFAULT_MODELS } from './utils/resources';

export { default as GigaEdgeEnhanceSDK } from './sdk';
export { default as GigaEdgeEnhanceError } from './utils/error';
export { LoadError } from './loader';
export { SetFeaturesFailedError } from './audioNode';

export type Features<M extends ModelsTmpl = typeof DEFAULT_MODELS> = RawFeatures<M>;
export type GigaEdgeEnhanceAudioNode<M extends ModelsTmpl = typeof DEFAULT_MODELS> = RawGigaEdgeEnhanceAudioNode<M>;
