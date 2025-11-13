import { ModelsTmpl, type Features as RawFeatures } from './utils/common';
import { type SpenchAudioNode as RawSpenchAudioNode } from './audioNode';
import { DEFAULT_MODELS } from './utils/resources';

export { default as SpenchSDK } from './sdk';
export { default as SpenchError } from './utils/error';
export { LoadError } from './loader';
export { SetFeaturesFailedError } from './audioNode';

export type Features<M extends ModelsTmpl = typeof DEFAULT_MODELS> = RawFeatures<M>;
export type SpenchAudioNode<M extends ModelsTmpl = typeof DEFAULT_MODELS> = RawSpenchAudioNode<M>;
