import { ModelsTmpl, type Features as RawFeatures } from './utils/common';
import { type GigaDenoiserAudioNode as RawGigaDenoiserAudioNode } from './audioNode';
import { DEFAULT_MODELS } from './utils/resources';

export { default as GigaDenoiserSDK } from './sdk';
export { default as GigaDenoiserError } from './utils/error';
export { LoadError } from './loader';
export { SetFeaturesFailedError } from './audioNode';

export type Features<M extends ModelsTmpl = typeof DEFAULT_MODELS> = RawFeatures<M>;
export type GigaDenoiserAudioNode<M extends ModelsTmpl = typeof DEFAULT_MODELS> = RawGigaDenoiserAudioNode<M>;
