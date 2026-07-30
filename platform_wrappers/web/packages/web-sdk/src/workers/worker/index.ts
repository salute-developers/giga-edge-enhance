import { handleControllerMessage, global } from './SpenchWorker';

global.addEventListener('message', handleControllerMessage);
