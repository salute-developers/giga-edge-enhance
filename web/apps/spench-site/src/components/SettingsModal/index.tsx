import { DEFAULT_SETTINGS, formatSettingValue, SETTINGS_DEFINITIONS, type SettingsDraft } from '../../settings';

type Props = {
    isOpen: boolean;
    isProcessing: boolean;
    settingsDraft: SettingsDraft;
    onClose: () => void;
    onApply: () => void;
    onChange: <T extends keyof SettingsDraft>(key: T, value: SettingsDraft[T]) => void;
};

const SettingsModal: React.FC<Props> = ({ isOpen, isProcessing, settingsDraft, onClose, onApply, onChange }) => {
    if (!isOpen) {
        return null;
    }

    return (
        <div className="settingsModalBackdrop" onClick={onClose}>
            <div aria-modal="true" className="settingsModal" onClick={(event) => event.stopPropagation()} role="dialog">
                <div className="settingsModalHeader">
                    <h2>Settings</h2>
                    <button type="button" onClick={onClose}>
                        Close
                    </button>
                </div>
                {isProcessing && (
                    <p className="settingsModalNote">
                        Processing is active. Applied values are saved now and used after the next restart.
                    </p>
                )}
                <div className="settingsList">
                    {SETTINGS_DEFINITIONS.map((definition) => (
                        <label className="settingsField" key={definition.key}>
                            <span className="settingsFieldHeader">
                                <span>{definition.label}</span>
                                <span className="settingsFieldDefault">
                                    Default: {formatSettingValue(DEFAULT_SETTINGS[definition.key])}
                                </span>
                            </span>
                            <span className="settingsFieldDescription">
                                Current: {settingsDraft[definition.key] || 'undefined'}
                            </span>
                            <span className="settingsFieldDescription">{definition.description}</span>
                            {definition.type === 'boolean' ? (
                                <select
                                    value={settingsDraft[definition.key]}
                                    onChange={(event) => onChange(definition.key, event.target.value)}
                                >
                                    <option value="true">true</option>
                                    <option value="false">false</option>
                                </select>
                            ) : definition.type === 'number' ? (
                                <input
                                    type="number"
                                    value={settingsDraft[definition.key]}
                                    onChange={(event) => onChange(definition.key, event.target.value)}
                                />
                            ) : (
                                <textarea
                                    rows={5}
                                    value={settingsDraft[definition.key]}
                                    onChange={(event) => onChange(definition.key, event.target.value)}
                                />
                            )}
                        </label>
                    ))}
                </div>
                <div className="settingsModalFooter">
                    <button type="button" onClick={onApply}>
                        Apply
                    </button>
                </div>
            </div>
        </div>
    );
};

export default SettingsModal;
