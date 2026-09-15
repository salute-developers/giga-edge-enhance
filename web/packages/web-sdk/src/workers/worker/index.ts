import { handleControllerMessage, global } from './GigaDenoiserWorker';

global.addEventListener('message', handleControllerMessage);
