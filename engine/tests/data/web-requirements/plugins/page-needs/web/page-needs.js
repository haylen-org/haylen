// Web part of the plugin of the requirement check, which requires the features of the permissions policy that the app names and answers with what the page lacks of them.
export default function load(context) {
    context.register("check", (params) => {
        try {
            context.require({ permissionsPolicy: params.features });
        } catch (error) {
            return { missing: error.data.missing.map((entry) => entry.name) };
        }
        return { missing: [] };
    });
}
