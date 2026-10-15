-- The person who uses the app: the profile, the saved addresses and the ways to pay.
local Account = {}

Account.name = 'Rosa Almeida'
Account.email = 'rosa@example.com'

Account.addresses = {
    {id = 'home', text = 'Home', line = '12 Harbor Lane, apartment 4', corner = {3, 13}},
    {id = 'work', text = 'Work', line = '480 Market Square, floor 9', corner = {11, 4}},
    {id = 'studio', text = 'Studio', line = '77 Garden Row', corner = {6, 6}},
}

Account.payments = {
    {id = 'card', text = 'Card ending in 4821'},
    {id = 'wallet', text = 'Phone wallet'},
    {id = 'cash', text = 'Cash on delivery'},
}

Account.address = 'home'
Account.favorites = {}

function Account.findAddress(id)
    for _, address in ipairs(Account.addresses) do
        if address.id == id then
            return address
        end
    end
end

function Account.currentAddress()
    return Account.findAddress(Account.address)
end

return Account
